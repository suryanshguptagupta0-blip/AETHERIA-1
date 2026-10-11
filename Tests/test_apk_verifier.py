"""Source-only parser regression tests using synthetic aapt2 text/ELF headers.

No APK is generated, signed, packaged or installed by these tests.
"""
import importlib.util
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / 'BuildScripts/verify_apk.py'
SPEC = importlib.util.spec_from_file_location('aetheria_apk_verifier', MODULE_PATH)
verifier = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(verifier)


def activity(name, orientation=None):
    result = f'    E: activity (line=12)\n      A: android:name(0x01010003)="{name}" (Raw: "{name}")\n'
    if orientation is not None:
        result += f'      A: android:screenOrientation(0x0101001e)=(type 0x10)0x{orientation:x}\n'
    return result


def manifest(*activities):
    return 'E: manifest (line=1)\n  E: application (line=4)\n' + ''.join(activities)


def valid_elf_header():
    header = bytearray(64)
    header[:7] = b'\x7fELF\x02\x01\x01'
    header[16:18] = (3).to_bytes(2, 'little')
    header[18:20] = (183).to_bytes(2, 'little')
    header[20:24] = (1).to_bytes(4, 'little')
    header[52:54] = (64).to_bytes(2, 'little')
    return header


class BadgingTests(unittest.TestCase):
    def test_expected_package_and_arm64(self):
        verifier.validate_badging("package: name='com.aetheria.fracturedrealities' versionCode='1'\nnative-code: 'arm64-v8a'\n")

    def test_reject_wrong_or_missing_package(self):
        for package in ('com.other.game', ''):
            with self.subTest(package=package), self.assertRaises(verifier.VerificationError):
                verifier.validate_badging(f"package: name='{package}'\nnative-code: 'arm64-v8a'\n")

    def test_reject_missing_or_wrong_abi(self):
        for line in ('', "native-code: 'armeabi-v7a'\n", "native-code: 'x86_64'\n"):
            with self.subTest(line=line), self.assertRaises(verifier.VerificationError):
                verifier.validate_badging("package: name='com.aetheria.fracturedrealities'\n" + line)

    def test_reject_ambiguous_reports(self):
        report = "package: name='com.aetheria.fracturedrealities'\nnative-code: 'arm64-v8a'\n"
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_badging(report + report)


class OrientationTests(unittest.TestCase):
    def test_accept_all_landscape_only_enums_including_zero(self):
        for orientation in (0, 6, 8, 11):
            with self.subTest(orientation=orientation):
                verifier.validate_game_activity_orientation(manifest(activity(verifier.GAME_ACTIVITY, orientation)))

    def test_reject_portrait_and_unconstrained_enums(self):
        for orientation in (1, 7, 9, 10, 12, 13, 14, 0xffffffff):
            with self.subTest(orientation=orientation), self.assertRaises(verifier.VerificationError):
                verifier.validate_game_activity_orientation(manifest(activity(verifier.GAME_ACTIVITY, orientation)))

    def test_other_activity_cannot_satisfy_requirement(self):
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_game_activity_orientation(manifest(activity('com.epicgames.unreal.SplashActivity', 6)))

    def test_portrait_game_with_landscape_splash_is_rejected(self):
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_game_activity_orientation(manifest(activity('com.epicgames.unreal.SplashActivity', 6), activity(verifier.GAME_ACTIVITY, 1)))

    def test_landscape_game_with_portrait_splash_is_accepted(self):
        verifier.validate_game_activity_orientation(manifest(activity('com.epicgames.unreal.SplashActivity', 1), activity(verifier.GAME_ACTIVITY, 6)))

    def test_orientation_is_not_inherited_from_application(self):
        xml = manifest(activity(verifier.GAME_ACTIVITY)).replace(
            '  E: application (line=4)\n',
            '  E: application (line=4)\n    A: android:screenOrientation(0x0101001e)=(type 0x10)0x6\n',
        )
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_game_activity_orientation(xml)

    def test_orientation_on_child_element_is_rejected(self):
        xml = manifest(activity(verifier.GAME_ACTIVITY)) + '      E: meta-data (line=15)\n        A: android:screenOrientation(0x0101001e)=(type 0x10)0x6\n'
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_game_activity_orientation(xml)

    def test_duplicate_game_activities_are_rejected(self):
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_game_activity_orientation(manifest(activity(verifier.GAME_ACTIVITY, 6), activity(verifier.GAME_ACTIVITY, 6)))

    def test_missing_orientation_is_rejected(self):
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_game_activity_orientation(manifest(activity(verifier.GAME_ACTIVITY)))

    def test_malformed_orientation_is_rejected(self):
        xml = manifest(activity(verifier.GAME_ACTIVITY, 6))
        for raw in ('(type 0x03)0x6', '"sensorLandscape"', '(type 0x10)0xg6'):
            with self.subTest(raw=raw), self.assertRaises(verifier.VerificationError):
                verifier.validate_game_activity_orientation(xml.replace('(type 0x10)0x6', raw))

    def test_duplicate_orientation_attributes_are_rejected(self):
        xml = manifest(activity(verifier.GAME_ACTIVITY, 6)) + '      A: android:screenOrientation(0x0101001e)=(type 0x10)0x1\n'
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_game_activity_orientation(xml)


class ElfTests(unittest.TestCase):
    def test_valid_arm64_shared_library_header(self):
        verifier.validate_elf_header(valid_elf_header())

    def test_short_header_is_rejected(self):
        with self.assertRaises(verifier.VerificationError):
            verifier.validate_elf_header(valid_elf_header()[:20])

    def test_corrupted_or_wrong_architecture_header_is_rejected(self):
        for offset, replacement in ((0, b'X'), (4, b'\x01'), (5, b'\x02'), (6, b'\x00'), (16, b'\x02'), (18, b'\x3e'), (20, b'\x00'), (52, b'\x20')):
            with self.subTest(offset=offset), self.assertRaises(verifier.VerificationError):
                header = valid_elf_header()
                header[offset:offset + len(replacement)] = replacement
                verifier.validate_elf_header(header)


if __name__ == '__main__':
    unittest.main()
