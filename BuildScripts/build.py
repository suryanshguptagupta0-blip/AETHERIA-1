#!/usr/bin/env python3
"""Run real UE5.6 Linux builds and Android packaging; never synthesize outputs."""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NDK_VERSION = '27.2.12479018'
BUILD_TOOLS_VERSION = '35.0.1'
CMAKE_VERSION = '3.22.1'
TARGETS = {'editor': ('AetheriaEditor', 'Editor'),
           'game': ('Aetheria', 'Game'), 'server': ('AetheriaServer', 'Server')}


class BuildError(RuntimeError):
    """A missing prerequisite or unverifiable build output."""


def require(condition, message):
    if not condition:
        raise BuildError(message)


def run(command, *, env=None, capture=False):
    print('Running:', ' '.join(str(value) for value in command), flush=True)
    result = subprocess.run([str(value) for value in command], cwd=ROOT, env=env,
                            check=True, text=True,
                            stdout=subprocess.PIPE if capture else None,
                            stderr=subprocess.STDOUT if capture else None)
    if capture:
        print(result.stdout, end='', flush=True)
    return result


def read_json(path):
    require(path.is_file(), f'Missing JSON receipt: {path}')
    require(path.stat().st_size <= 1024 * 1024, f'Oversized JSON receipt: {path}')
    try:
        value = json.loads(path.read_text())
    except (OSError, ValueError) as error:
        raise BuildError(f'Cannot read JSON receipt {path}: {error}') from error
    require(isinstance(value, dict), f'Receipt must be a JSON object: {path}')
    return value


def engine_preflight(ue_root, *, android=False):
    require(ue_root, 'UE_ROOT or --ue-root must point to a real Unreal Engine 5.6 installation. No native build was attempted.')
    ue = Path(ue_root).expanduser().resolve()
    version = read_json(ue / 'Engine/Build/Build.version')
    require((version.get('MajorVersion'), version.get('MinorVersion')) == (5, 6),
            'This source targets UE5.6; validate upgrades before using another engine version.')
    if android:
        patch = version.get('PatchVersion')
        require(isinstance(patch, int) and patch >= 1,
                'Android requires UE5.6.1 or a later 5.6 patch; UE5.6.0 is not supported by this toolchain gate.')
    for relative in ('Engine/Build/BatchFiles/Linux/Build.sh',
                     'Engine/Build/BatchFiles/RunUAT.sh'):
        require((ue / relative).is_file(), f'Missing Unreal build tool: {ue / relative}')
    return ue, version


def toolchain_paths(environment):
    sdk_value = environment.get('ANDROID_HOME') or environment.get('ANDROID_SDK_ROOT')
    require(sdk_value, 'Configure ANDROID_HOME (or ANDROID_SDK_ROOT) for the installed Android SDK.')
    sdk = Path(sdk_value).expanduser().resolve()
    ndk_value = environment.get('NDKROOT') or environment.get('ANDROID_NDK_ROOT')
    ndk = Path(ndk_value).expanduser().resolve() if ndk_value else sdk / 'ndk' / NDK_VERSION
    java_home = environment.get('JAVA_HOME')
    java = Path(java_home).expanduser() / 'bin/java' if java_home else shutil.which('java', path=environment.get('PATH'))
    require(java, 'Java 21 JDK is required; configure JAVA_HOME or PATH.')
    java = Path(java).resolve()
    return {'sdk': sdk, 'ndk': ndk, 'java': java, 'javac': java.parent / 'javac',
            'clang': ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/clang',
            'adb': sdk / 'platform-tools/adb',
            'aapt2': sdk / 'build-tools' / BUILD_TOOLS_VERSION / 'aapt2',
            'apksigner': sdk / 'build-tools' / BUILD_TOOLS_VERSION / 'apksigner',
            'cmake': sdk / 'cmake' / CMAKE_VERSION / 'bin/cmake'}


def validate_toolchain_files(paths):
    require((paths['sdk'] / 'platforms/android-34/android.jar').is_file(),
            'Android SDK platform 34 is required by this UE5.6.1 toolchain gate.')
    properties = paths['ndk'] / 'source.properties'
    require(properties.is_file(), f'NDK source.properties is missing: {properties}')
    revision = re.search(r'^\s*Pkg\.Revision\s*=\s*([^\s]+)\s*$',
                         properties.read_text(), re.MULTILINE)
    require(revision and revision.group(1) == NDK_VERSION,
            f'Expected NDK r27c ({NDK_VERSION}); validate changes with Unreal Turnkey.')
    for name in ('java', 'javac', 'clang', 'adb', 'aapt2', 'apksigner', 'cmake'):
        require(paths[name].is_file() and os.access(paths[name], os.X_OK),
                f'Missing executable {name}: {paths[name]}')


def toolchain_preflight(environment=None):
    environment = dict(os.environ if environment is None else environment)
    paths = toolchain_paths(environment)
    validate_toolchain_files(paths)
    environment.update(ANDROID_HOME=str(paths['sdk']), ANDROID_SDK_ROOT=str(paths['sdk']),
                       NDKROOT=str(paths['ndk']), ANDROID_NDK_ROOT=str(paths['ndk']),
                       JAVA_HOME=str(paths['java'].parent.parent))
    versions = {}
    for name, argument in (('java', '-version'), ('javac', '-version'),
                           ('clang', '--version'), ('adb', 'version'),
                           ('aapt2', 'version'), ('apksigner', 'version'),
                           ('cmake', '--version')):
        versions[name] = run([paths[name], argument], env=environment, capture=True).stdout.strip()
    require(re.search(r'version\s+"21(?:[.\-+\s"]|$)', versions['java']),
            'Expected a Java 21 runtime.')
    require(re.search(r'^javac\s+21(?:[.\-+\s]|$)', versions['javac']),
            'Expected a Java 21 compiler from the same JDK.')
    require(re.search(r'^cmake version 3\.22\.1(?:[-+\s]|$)', versions['cmake']),
            f'Expected Android SDK CMake {CMAKE_VERSION}.')
    return paths, environment, versions


def receipt_product_path(raw_path, project, ue):
    require(isinstance(raw_path, str) and raw_path, 'UBT BuildProduct Path must be a nonempty string.')
    expanded = raw_path.replace('$(ProjectDir)', str(project)).replace('$(EngineDir)', str(ue / 'Engine'))
    require('$(' not in expanded, f'Unresolved UBT path variable: {raw_path}')
    path = Path(expanded)
    if not path.is_absolute():
        path = project / path
    return path.resolve()


def require_linux_elf(path):
    with path.open('rb') as stream:
        header = stream.read(20)
    require(len(header) == 20 and header[:4] == b'\x7fELF' and header[4:6] == b'\x02\x01'
            and int.from_bytes(header[18:20], 'little') in (62, 183),
            f'Expected a real 64-bit Linux ELF build product: {path}')


def validate_target_receipt(receipt_path, project, ue, target, target_type, configuration):
    receipt = read_json(receipt_path)
    for key, expected in (('TargetName', target), ('Platform', 'Linux'), ('Configuration', configuration)):
        require(receipt.get(key) == expected, f'UBT receipt {key} does not match {expected}: {receipt_path}')
    # TargetType is present in some receipt versions; do not assume an untested schema.
    if 'TargetType' in receipt:
        require(receipt['TargetType'] == target_type, f'Wrong UBT TargetType: {receipt_path}')
    products = receipt.get('BuildProducts')
    require(isinstance(products, list) and products, f'No BuildProducts in UBT receipt: {receipt_path}')
    required_type = 'DynamicLibrary' if target_type == 'Editor' else 'Executable'
    verified = []
    binaries = (project / 'Binaries/Linux').resolve()
    for product in products:
        require(isinstance(product, dict) and isinstance(product.get('Type'), str),
                f'Malformed BuildProduct: {receipt_path}')
        path = receipt_product_path(product.get('Path'), project, ue)
        require(path.is_file() and path.stat().st_size > 0, f'UBT BuildProduct is missing or empty: {path}')
        if product['Type'] == required_type and path.is_relative_to(binaries):
            if target_type == 'Editor' and not path.name.startswith('libUnrealEditor-Aetheria'):
                continue
            require_linux_elf(path)
            if required_type == 'Executable':
                require(os.access(path, os.X_OK), f'Game/Server build product is not executable: {path}')
            verified.append(path)
    require(verified, f'UBT receipt has no project {required_type} for the {target_type} target: {receipt_path}')
    return {'receipt': str(receipt_path), 'target': target, 'configuration': configuration,
            'verified_products': [str(path) for path in verified]}


def find_target_receipt(project, ue, target, target_type, configuration):
    candidates = []
    for path in sorted((project / 'Binaries/Linux').glob('*.target')):
        try:
            receipt = read_json(path)
        except BuildError:
            continue
        if (receipt.get('TargetName'), receipt.get('Platform'), receipt.get('Configuration')) == (target, 'Linux', configuration):
            candidates.append(path)
    require(candidates, f'UBT returned without a matching {target}/Linux/{configuration} .target receipt. Arbitrary or stale files cannot prove this build.')
    require(len(candidates) == 1, f'Ambiguous target receipts for {target}: {candidates}')
    return validate_target_receipt(candidates[0], project, ue, target, target_type, configuration)


def build_native(ue, name, configuration, environment=None):
    require(name in TARGETS, f'Unknown native target: {name}')
    if name == 'server':
        require(not (ue / 'Engine/Build/InstalledBuild.txt').exists(),
                'Dedicated Server requires a source-built engine with server targets; this is an installed build.')
    target, target_type = TARGETS[name]
    run(['bash', ue / 'Engine/Build/BatchFiles/Linux/Build.sh', target, 'Linux', configuration,
         ROOT / 'Aetheria.uproject', '-WaitMutex', '-NoHotReload'], env=environment)
    evidence = find_target_receipt(ROOT, ue, target, target_type, configuration)
    print(json.dumps(evidence, indent=2))
    return evidence


def turnkey_android(ue, environment):
    result = run(['bash', ue / 'Engine/Build/BatchFiles/RunUAT.sh', 'Turnkey',
                  '-command=VerifySdk', '-platform=Android', f'-project={ROOT / "Aetheria.uproject"}',
                  '-utf8output'], env=environment, capture=True)
    log = ROOT / 'Saved/BuildEvidence/turnkey-android.log'
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(result.stdout)
    require(re.search(r'Android\s*:\s*\([^\r\n]*\bStatus=Valid\b', result.stdout),
            f'Unreal Turnkey did not report Android Status=Valid. Review the actual engine log: {log}')
    print('Installed Unreal Engine Turnkey confirmed Android SDK compatibility.')


def validate_avatar_receipt(receipt, project, version):
    evidence = read_json(receipt)
    require(evidence.get('schema_version') == 1 and evidence.get('status') == 'imported_and_saved',
            'Unreal avatar importer did not confirm imported and saved assets.')
    require(evidence.get('project') == str(project), 'Avatar receipt belongs to another project.')
    require(evidence.get('engine_version') == version, 'Avatar receipt engine version does not match the installed engine.')
    mode = '/Game/Core/BP_AetheriaGameMode.BP_AetheriaGameMode_C'
    pawn = '/Game/Characters/BP_AetheriaTraveler.BP_AetheriaTraveler_C'
    require(evidence.get('game_mode') == mode and evidence.get('pawn') == pawn,
            'Avatar receipt does not identify the required GameMode and Traveler Blueprint classes.')
    assets = evidence.get('assets')
    require(isinstance(assets, list) and len(assets) == 10, 'Avatar receipt must identify two meshes, six animations and two Blueprints.')
    files = set()
    expected_files = {'Content/Core/BP_AetheriaGameMode.uasset',
                      'Content/Characters/BP_AetheriaTraveler.uasset'}
    for sex in ('Male', 'Female'):
        expected_files.add(f'Content/Characters/Imported/SK_Rocketbox{sex}.uasset')
        for label in ('Idle', 'Walk', 'Run'):
            expected_files.add(f'Content/Animations/Imported/A_{sex}{label}.uasset')
    for asset in assets:
        require(isinstance(asset, dict) and isinstance(asset.get('object_path'), str)
                and isinstance(asset.get('package_file'), str), 'Malformed imported asset receipt.')
        object_path = asset['object_path']
        require(object_path.startswith('/Game/') and '.' in object_path, 'Unexpected imported Unreal object path.')
        expected = 'Content/' + object_path.split('.')[0].removeprefix('/Game/') + '.uasset'
        require(asset['package_file'] == expected and object_path.rsplit('.', 1)[1] == Path(expected).stem,
                'Imported asset package does not match its object path.')
        file = (project / expected).resolve()
        require(file.is_relative_to((project / 'Content').resolve()) and file.is_file()
                and file.stat().st_size > 0, f'Imported Unreal asset is missing: {file}')
        files.add(expected)
    require(files == expected_files and len(files) == 10, 'Duplicate, unexpected or missing required avatar packages.')
    config = (project / 'Config/DefaultEngine.ini').read_text()
    for key in ('GlobalDefaultGameMode', 'GlobalDefaultServerGameMode'):
        values = re.findall(rf'^{key}=([^\r\n]+)$', config, re.MULTILINE)
        require(values == [mode], f'Imported avatar GameMode is not active in {key}.')
    game_config = (project / 'Config/DefaultGame.ini').read_text()
    for directory in ('/Game/Core', '/Game/Characters', '/Game/Animations'):
        require(re.search(rf'^\+DirectoriesToAlwaysCook=\(Path="{re.escape(directory)}"\)$',
                          game_config, re.MULTILINE), f'Imported avatars are missing a cook directory: {directory}')
    return evidence


def prepare_avatars(ue, version, environment=None):
    build_native(ue, 'editor', 'Development', environment)
    editor = ue / 'Engine/Binaries/Linux/UnrealEditor-Cmd'
    require(editor.is_file() and os.access(editor, os.X_OK), f'Missing real UnrealEditor-Cmd: {editor}')
    receipt = ROOT / 'Saved/BuildEvidence/avatar-import.json'
    started = time.time()
    run([editor, ROOT / 'Aetheria.uproject', '-unattended', '-nop4', '-NullRHI',
         '-run=pythonscript', f'-script={ROOT / "BuildScripts/import_art.py"}', '-stdout',
         '-FullStdOutLogOutput'], env=environment)
    require(receipt.is_file() and receipt.stat().st_mtime >= started,
            'Unreal avatar import did not write a new receipt. No imported avatar claim is permitted.')
    validate_avatar_receipt(receipt, ROOT, version)
    print('Unreal Editor imported, saved and activated avatar assets. Appearance and animations still require gameplay QA.')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=['check', 'toolchain', 'editor', 'game', 'server', 'avatars', 'android'])
    parser.add_argument('--ue-root', default=os.environ.get('UE_ROOT'))
    parser.add_argument('--configuration', choices=['Development', 'Shipping'], default='Development')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(argv)
    try:
        run([sys.executable, ROOT / 'Tests/validate_project.py'])
        if args.target == 'toolchain':
            paths, environment, versions = toolchain_preflight()
            result = {'toolchain_probes': 'passed', 'sdk': str(paths['sdk']), 'ndk': str(paths['ndk']),
                      'versions': versions, 'unreal_compatibility': 'not_run', 'native_build': 'not_run', 'apk': 'not_built'}
            if args.ue_root:
                ue, _ = engine_preflight(args.ue_root, android=True)
                turnkey_android(ue, environment)
                result['unreal_compatibility'] = 'Turnkey_VerifySdk_passed'
            print(json.dumps(result, indent=2))
            return 0
        ue, version = engine_preflight(args.ue_root, android=args.target == 'android')
        if args.target == 'check':
            print(json.dumps({'ue_preflight': 'passed', 'version': version, 'native_build': 'not_run'}))
        elif args.target in TARGETS:
            build_native(ue, args.target, args.configuration)
            print('UBT and target-specific products verified. Cooking, gameplay and multiplayer QA remain separate gates.')
        elif args.target == 'avatars':
            prepare_avatars(ue, version)
        else:
            paths, environment, _ = toolchain_preflight()
            turnkey_android(ue, environment)
            prepare_avatars(ue, version, environment)
            output = (args.output or ROOT / 'Build/Artifacts' / time.strftime('%Y%m%d-%H%M%S')).resolve()
            output.mkdir(parents=True, exist_ok=True)
            started = time.time()
            run(['bash', ue / 'Engine/Build/BatchFiles/RunUAT.sh', 'BuildCookRun',
                 f'-project={ROOT / "Aetheria.uproject"}', '-noP4', '-platform=Android',
                 f'-clientconfig={args.configuration}', '-build', '-cook', '-map=/Engine/Maps/Entry',
                 '-stage', '-pak', '-package', '-archive', f'-archivedirectory={output}',
                 '-cookflavor=ASTC'], env=environment)
            apks = [file for file in output.rglob('*.apk') if file.stat().st_mtime >= started]
            require(apks, 'UAT returned without a new APK in the archive. No APK release claim is permitted.')
            for apk in apks:
                run([sys.executable, ROOT / 'BuildScripts/verify_apk.py', apk, '--sdk', paths['sdk']], env=environment)
            print('New signed ARM64 Unreal APK verified. Device install, touch controls and frame captures remain required.')
        return 0
    except (BuildError, OSError) as error:
        print(f'Build prerequisite/output error: {error}', file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, end='', file=sys.stderr)
        print(f'Real build/tool command failed with exit {error.returncode}. No successful build is claimed.', file=sys.stderr)
        return error.returncode if error.returncode > 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
