#!/usr/bin/env python3
"""Verify actual APK signatures, game activity orientation and ARM64 ELF.

Parsing helpers are import-safe so source tests can exercise rejection cases
without creating an APK or claiming an Unreal package was built.
"""
import argparse
import re
import subprocess
import zipfile
from pathlib import Path

EXPECTED_PACKAGE = 'com.aetheria.fracturedrealities'
GAME_ACTIVITY = 'com.epicgames.unreal.GameActivity'
LANDSCAPE_ORIENTATIONS = frozenset((0, 6, 8, 11))


class VerificationError(ValueError):
    """A real APK or one of its tool reports failed a required check."""


def validate_badging(badging):
    packages = re.findall(r"^package:\s+name='([^']+)'(?:\s|$)", badging, re.MULTILINE)
    if packages != [EXPECTED_PACKAGE]:
        raise VerificationError('Unexpected or missing APK package identity')
    abi_lines = re.findall(r'^native-code:\s*(.*)$', badging, re.MULTILINE)
    if len(abi_lines) != 1 or 'arm64-v8a' not in re.findall(r"'([^']+)'", abi_lines[0]):
        raise VerificationError('Missing ARM64 native ABI in APK badging')


def validate_game_activity_orientation(xml):
    """Read attributes owned by the exact UE5 GameActivity XML element.

    aapt2 xmltree uses indentation to associate attributes and nested elements.
    An orientation on a splash activity, application or child element cannot
    satisfy the game activity requirement. Android enum 1 is portrait; enum 0
    is landscape. Only 0/6/8/11 constrain the game to landscape orientations.
    """
    stack, activities = [], []
    element_pattern = re.compile(r'^([ \t]*)E:\s+(\S+)')
    attribute_pattern = re.compile(
        r'^[ \t]*A:\s+(android:(?:name|screenOrientation))(?:\([^)]*\))?\s*=\s*(.*)$'
    )
    for line in xml.splitlines():
        element = element_pattern.match(line)
        if element:
            depth = len(element.group(1).expandtabs(8))
            while stack and stack[-1][0] >= depth:
                stack.pop()
            attributes = {}
            stack.append((depth, element.group(2), attributes))
            if element.group(2) == 'activity':
                activities.append(attributes)
            continue
        attribute = attribute_pattern.match(line)
        if not attribute or not stack or stack[-1][1] != 'activity':
            continue
        key, raw = attribute.groups()
        attributes = stack[-1][2]
        if key in attributes:
            raise VerificationError('Duplicate game activity XML attribute')
        attributes[key] = raw

    game_activities = []
    for attributes in activities:
        name = re.match(r'^"([^"\r\n]+)"(?:\s|$)', attributes.get('android:name', ''))
        if name and name.group(1) == GAME_ACTIVITY:
            game_activities.append(attributes)
    if len(game_activities) != 1:
        raise VerificationError('Expected exactly one UE5 GameActivity in binary manifest')
    raw_orientation = game_activities[0].get('android:screenOrientation', '')
    orientation = re.match(r'^(?:\(type 0x10\))?\s*0x([0-9a-fA-F]+)(?:\s|$)', raw_orientation)
    if not orientation or int(orientation.group(1), 16) not in LANDSCAPE_ORIENTATIONS:
        raise VerificationError('Landscape orientation not confirmed on UE5 GameActivity')


def validate_elf_header(header, name='<library>'):
    if (
        len(header) < 64
        or header[:4] != b'\x7fELF'
        or header[4] != 2  # ELFCLASS64
        or header[5] != 1  # ELFDATA2LSB
        or header[6] != 1  # EI_VERSION
        or int.from_bytes(header[16:18], 'little') != 3  # ET_DYN shared object
        or int.from_bytes(header[18:20], 'little') != 183  # EM_AARCH64
        or int.from_bytes(header[20:24], 'little') != 1  # EV_CURRENT
        or int.from_bytes(header[52:54], 'little') != 64  # ELF64 header size
    ):
        raise VerificationError(f'{name} is not a valid 64-bit little-endian AArch64 shared-library header')


def verify_apk(apk, sdk):
    apk, sdk = Path(apk), Path(sdk)
    if not apk.is_file() or not zipfile.is_zipfile(apk):
        raise VerificationError('Input is not a real APK ZIP archive')
    build_tools = sorted(
        (sdk / 'build-tools').glob('*'),
        key=lambda path: [int(n) for n in re.findall(r'\d+', path.name)],
    )
    if not build_tools:
        raise VerificationError('SDK build-tools are missing')
    base = build_tools[-1]
    for name in ('apksigner', 'aapt2'):
        if not (base / name).is_file():
            raise VerificationError(f'{name} is unavailable')
    subprocess.run([str(base / 'apksigner'), 'verify', '--verbose', str(apk)], check=True)
    badging = subprocess.check_output([str(base / 'aapt2'), 'dump', 'badging', str(apk)], text=True)
    validate_badging(badging)
    xml = subprocess.check_output(
        [str(base / 'aapt2'), 'dump', 'xmltree', str(apk), '--file', 'AndroidManifest.xml'], text=True
    )
    validate_game_activity_orientation(xml)
    with zipfile.ZipFile(apk) as archive:
        libraries = [name for name in archive.namelist() if name.startswith('lib/arm64-v8a/') and name.endswith('.so')]
        if 'lib/arm64-v8a/libUnreal.so' not in libraries:
            raise VerificationError('No Unreal ARM64 shared library in the APK')
        for name in libraries:
            with archive.open(name) as stream:
                validate_elf_header(stream.read(64), name)
    print(f'Verified APK: {apk.name}; signature/identity/game-activity landscape/ARM64 ELF checks passed. No device run claimed.')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--sdk', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        verify_apk(args.apk, args.sdk)
    except (VerificationError, OSError, zipfile.BadZipFile) as error:
        parser.error(str(error))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
