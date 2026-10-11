#!/usr/bin/env python3
"""Fail-closed launcher tests using synthetic receipts, never UE build evidence."""
import importlib.util
import contextlib
import io
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

MODULE = Path(__file__).resolve().parents[1] / 'BuildScripts/build.py'
spec = importlib.util.spec_from_file_location('aetheria_build', MODULE)
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class BuildPreflightTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.base = Path(self.directory.name)
        self.project = self.base / 'project'
        self.ue = self.base / 'engine'
        self.binaries = self.project / 'Binaries/Linux'
        self.binaries.mkdir(parents=True)
        self.version = {'MajorVersion': 5, 'MinorVersion': 6, 'PatchVersion': 1}
        version_file = self.ue / 'Engine/Build/Build.version'
        version_file.parent.mkdir(parents=True)
        version_file.write_text(json.dumps(self.version))
        for relative in ('Engine/Build/BatchFiles/Linux/Build.sh', 'Engine/Build/BatchFiles/RunUAT.sh'):
            file = self.ue / relative
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text('# synthetic tool fixture; not run\n')

    def receipt(self, target='Aetheria', configuration='Development', products=None, **extra):
        receipt = self.binaries / f'{target}.target'
        data = {'TargetName': target, 'Platform': 'Linux', 'Configuration': configuration,
                'BuildProducts': products if products is not None else []}
        data.update(extra)
        receipt.write_text(json.dumps(data))
        return receipt

    def product(self, name='Aetheria', elf=True):
        path = self.binaries / name
        # Minimal ELF identification fixture only; never used as build output.
        header = bytearray(20)
        header[:6] = b'\x7fELF\x02\x01'
        header[18:20] = (62).to_bytes(2, 'little')
        path.write_bytes(header if elf else b'not an ELF binary')
        path.chmod(0o755)
        return path

    def validate(self, receipt, target='Aetheria', kind='Game', configuration='Development'):
        return build.validate_target_receipt(receipt, self.project, self.ue, target, kind, configuration)

    def test_arbitrary_stale_aetheria_file_cannot_prove_native_build(self):
        arbitrary = self.binaries / 'Old-Aetheria-placeholder.txt'
        arbitrary.write_text('stale output')
        os.utime(arbitrary, (1, 1))
        with self.assertRaisesRegex(build.BuildError, 'matching'):
            build.find_target_receipt(self.project, self.ue, 'Aetheria', 'Game', 'Development')

    def test_editor_receipt_cannot_prove_game_or_server_build(self):
        path = self.product('libUnrealEditor-Aetheria.so')
        self.receipt('AetheriaEditor', products=[{'Path': str(path), 'Type': 'DynamicLibrary'}])
        for target, kind in [('Aetheria', 'Game'), ('AetheriaServer', 'Server')]:
            with self.subTest(target=target), self.assertRaises(build.BuildError):
                build.find_target_receipt(self.project, self.ue, target, kind, 'Development')

    def test_wrong_platform_or_configuration_rejected(self):
        for field, value in [('Platform', 'Win64'), ('Configuration', 'Shipping'), ('TargetName', 'AetheriaServer')]:
            data = {'TargetName': 'Aetheria', 'Platform': 'Linux', 'Configuration': 'Development', 'BuildProducts': []}
            data[field] = value
            receipt = self.binaries / 'wrong.target'
            receipt.write_text(json.dumps(data))
            with self.subTest(field=field), self.assertRaises(build.BuildError):
                self.validate(receipt)

    def test_missing_or_empty_declared_product_rejected(self):
        path = self.binaries / 'Aetheria'
        receipt = self.receipt(products=[{'Path': str(path), 'Type': 'Executable'}])
        with self.assertRaisesRegex(build.BuildError, 'missing or empty'):
            self.validate(receipt)
        path.touch()
        with self.assertRaisesRegex(build.BuildError, 'missing or empty'):
            self.validate(receipt)

    def test_non_elf_or_non_executable_native_product_rejected(self):
        path = self.product(elf=False)
        receipt = self.receipt(products=[{'Path': str(path), 'Type': 'Executable'}])
        with self.assertRaisesRegex(build.BuildError, 'ELF'):
            self.validate(receipt)
        self.product().chmod(0o644)
        with self.assertRaisesRegex(build.BuildError, 'not executable'):
            self.validate(receipt)

    def test_valid_receipt_can_omit_optional_target_type(self):
        self.product()
        receipt = self.receipt(products=[{'Path': '$(ProjectDir)/Binaries/Linux/Aetheria', 'Type': 'Executable'}])
        result = self.validate(receipt)
        self.assertEqual(result['target'], 'Aetheria')
        self.assertEqual(result['verified_products'], [str(self.binaries / 'Aetheria')])

    def test_wrong_optional_target_type_rejected(self):
        receipt = self.receipt(TargetType='Server')
        with self.assertRaisesRegex(build.BuildError, 'TargetType'):
            self.validate(receipt)

    def test_library_alone_cannot_prove_game_binary(self):
        path = self.product('libUnrealEditor-Aetheria.so')
        receipt = self.receipt(products=[{'Path': str(path), 'Type': 'DynamicLibrary'}])
        with self.assertRaisesRegex(build.BuildError, 'no project Executable'):
            self.validate(receipt)

    def test_unknown_receipt_path_variable_rejected(self):
        with self.assertRaisesRegex(build.BuildError, 'Unresolved'):
            build.receipt_product_path('$(Unknown)/Aetheria', self.project, self.ue)

    def test_android_rejects_ue_560_but_linux_preflight_allows_it(self):
        self.version['PatchVersion'] = 0
        (self.ue / 'Engine/Build/Build.version').write_text(json.dumps(self.version))
        build.engine_preflight(self.ue)
        with self.assertRaisesRegex(build.BuildError, '5.6.1'):
            build.engine_preflight(self.ue, android=True)

    def test_other_engine_minor_rejected(self):
        self.version['MinorVersion'] = 7
        (self.ue / 'Engine/Build/Build.version').write_text(json.dumps(self.version))
        with self.assertRaisesRegex(build.BuildError, 'UE5.6'):
            build.engine_preflight(self.ue)

    def test_sdk_derived_ndk_when_ndkroot_missing(self):
        sdk = self.base / 'sdk'
        paths = build.toolchain_paths({'ANDROID_HOME': str(sdk), 'JAVA_HOME': str(self.base / 'jdk')})
        self.assertEqual(paths['ndk'], sdk / 'ndk/27.2.12479018')
        explicit = self.base / 'custom-ndk'
        paths = build.toolchain_paths({'ANDROID_HOME': str(sdk), 'JAVA_HOME': str(self.base / 'jdk'), 'NDKROOT': str(explicit)})
        self.assertEqual(paths['ndk'], explicit)

    def test_ndk_revision_must_be_exact(self):
        sdk = self.base / 'sdk'
        paths = build.toolchain_paths({'ANDROID_HOME': str(sdk), 'JAVA_HOME': str(self.base / 'jdk')})
        jar = sdk / 'platforms/android-34/android.jar'
        jar.parent.mkdir(parents=True)
        jar.write_bytes(b'test fixture')
        properties = paths['ndk'] / 'source.properties'
        properties.parent.mkdir(parents=True)
        properties.write_text('Pkg.Revision = 27.2.124790180\n')
        with self.assertRaisesRegex(build.BuildError, 'r27c'):
            build.validate_toolchain_files(paths)

    def test_official_sdk_cmake_build_suffix_accepted(self):
        versions = {'java': 'openjdk version "21.0.11" 2026-04-21', 'javac': 'javac 21.0.11',
                    'cmake': 'cmake version 3.22.1-g37088a8', 'clang': 'Android clang',
                    'adb': 'Android Debug Bridge', 'aapt2': 'Android Asset Packaging Tool',
                    'apksigner': '0.9'}
        paths = {name: self.base / name for name in ('java', 'javac', 'clang', 'adb', 'aapt2', 'apksigner', 'cmake', 'sdk', 'ndk')}
        def probe(command, **kwargs):
            return build.subprocess.CompletedProcess(command, 0, stdout=versions[command[0].name])
        with mock.patch.object(build, 'toolchain_paths', return_value=paths), mock.patch.object(build, 'validate_toolchain_files'), mock.patch.object(build, 'run', side_effect=probe):
            _, _, checked = build.toolchain_preflight({})
            self.assertEqual(checked['cmake'], versions['cmake'])
            versions['cmake'] = 'cmake version 3.22.10'
            with self.assertRaisesRegex(build.BuildError, 'CMake'):
                build.toolchain_preflight({})

    def test_native_product_check_only_after_ubt_success(self):
        error = build.subprocess.CalledProcessError(7, ['UBT'])
        with mock.patch.object(build, 'run', side_effect=error), mock.patch.object(build, 'find_target_receipt') as receipt:
            with self.assertRaises(build.subprocess.CalledProcessError):
                build.build_native(self.ue, 'game', 'Development')
            receipt.assert_not_called()

    def test_avatar_receipt_from_other_project_rejected(self):
        path = self.base / 'avatar-import.json'
        path.write_text(json.dumps({'schema_version': 1, 'status': 'imported_and_saved', 'project': '/different'}))
        with self.assertRaisesRegex(build.BuildError, 'another project'):
            build.validate_avatar_receipt(path, self.project, self.version)

    def avatar_receipt(self):
        packages = ['Core/BP_AetheriaGameMode', 'Characters/BP_AetheriaTraveler']
        for sex in ('Male', 'Female'):
            packages.append(f'Characters/Imported/SK_Rocketbox{sex}')
            for label in ('Idle', 'Walk', 'Run'):
                packages.append(f'Animations/Imported/A_{sex}{label}')
        assets = []
        for package in packages:
            file = self.project / f'Content/{package}.uasset'
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'synthetic package fixture; never Unreal output')
            assets.append({'object_path': f'/Game/{package}.{Path(package).name}',
                           'package_file': f'Content/{package}.uasset'})
        mode = '/Game/Core/BP_AetheriaGameMode.BP_AetheriaGameMode_C'
        config = self.project / 'Config'
        config.mkdir()
        (config / 'DefaultEngine.ini').write_text(f'GlobalDefaultGameMode={mode}\nGlobalDefaultServerGameMode={mode}\n')
        (config / 'DefaultGame.ini').write_text(''.join(f'+DirectoriesToAlwaysCook=(Path="{directory}")\n'
                                                       for directory in ('/Game/Core', '/Game/Characters', '/Game/Animations')))
        data = {'schema_version': 1, 'status': 'imported_and_saved', 'project': str(self.project),
                'engine_version': self.version, 'assets': assets, 'game_mode': mode,
                'pawn': '/Game/Characters/BP_AetheriaTraveler.BP_AetheriaTraveler_C'}
        receipt = self.base / 'avatar-import.json'
        receipt.write_text(json.dumps(data))
        return receipt, data

    def test_exact_avatar_receipt_shape_and_duplicate_rejection(self):
        receipt, data = self.avatar_receipt()
        self.assertEqual(build.validate_avatar_receipt(receipt, self.project, self.version)['status'], 'imported_and_saved')
        data['assets'][-1] = data['assets'][-2]
        receipt.write_text(json.dumps(data))
        with self.assertRaisesRegex(build.BuildError, 'Duplicate'):
            build.validate_avatar_receipt(receipt, self.project, self.version)

    def test_missing_avatar_package_or_inactive_mode_rejected(self):
        receipt, data = self.avatar_receipt()
        (self.project / data['assets'][0]['package_file']).unlink()
        with self.assertRaisesRegex(build.BuildError, 'asset is missing'):
            build.validate_avatar_receipt(receipt, self.project, self.version)
        (self.project / data['assets'][0]['package_file']).write_bytes(b'synthetic fixture')
        (self.project / 'Config/DefaultEngine.ini').write_text(';GlobalDefaultGameMode=' + data['game_mode'] + '\n')
        with self.assertRaisesRegex(build.BuildError, 'not active'):
            build.validate_avatar_receipt(receipt, self.project, self.version)

    def test_stale_avatar_receipt_cannot_prove_current_import(self):
        receipt = self.project / 'Saved/BuildEvidence/avatar-import.json'
        receipt.parent.mkdir(parents=True)
        receipt.write_text('{}')
        os.utime(receipt, (1, 1))
        editor = self.ue / 'Engine/Binaries/Linux/UnrealEditor-Cmd'
        editor.parent.mkdir(parents=True)
        editor.write_text('synthetic executable fixture; not run\n')
        editor.chmod(0o755)
        with mock.patch.object(build, 'ROOT', self.project), mock.patch.object(build, 'build_native'), mock.patch.object(build, 'run'):
            with self.assertRaisesRegex(build.BuildError, 'new receipt'):
                build.prepare_avatars(self.ue, self.version)

    def test_toolchain_target_never_claims_unreal_or_apk_without_engine(self):
        versions = {'java': 'test fixture'}
        paths = {'sdk': self.base / 'sdk', 'ndk': self.base / 'ndk'}
        with contextlib.redirect_stdout(io.StringIO()), mock.patch.dict(os.environ, {}, clear=True), mock.patch.object(build, 'run'), mock.patch.object(build, 'toolchain_preflight', return_value=(paths, {}, versions)), mock.patch.object(build, 'turnkey_android') as turnkey:
            self.assertEqual(build.main(['toolchain']), 0)
            turnkey.assert_not_called()


if __name__ == '__main__':
    unittest.main()
