#!/usr/bin/env python3
"""Install the free official Android toolchain; does not install Unreal Engine.

Requires Java 21 and explicit --accept-licenses before sdkmanager can install.
Command-line tools are downloaded from Google's repository and checksum checked.
No credentials, cloud billable resources or engine mirrors are used.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

PACKAGES = ('platform-tools', 'platforms;android-34', 'build-tools;35.0.1', 'ndk;27.2.12479018', 'cmake;3.22.1')
BASE = 'https://dl.google.com/android/repository/'
CLI_PACKAGE = 'cmdline-tools;13.0'


def run(command, **kwargs):
    return subprocess.run([str(part) for part in command], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--accept-licenses', action='store_true', help='Accept Android SDK package license prompts for this build toolchain.')
    parser.add_argument('--bootstrap-only', action='store_true', help='Download and verify the pinned command-line tools without installing packages.')
    args = parser.parse_args()
    if not args.accept_licenses:
        parser.error('Review Android SDK terms and supply --accept-licenses to install packages.')
    java_home = os.environ.get('JAVA_HOME')
    java = Path(java_home) / 'bin/java' if java_home else shutil.which('java')
    if not java:
        parser.error('Java 21 is required. Install a free OpenJDK 21 distribution first.')
    java_version = run([java, '-version'], capture_output=True, text=True)
    if 'version "21.' not in java_version.stderr:
        parser.error('Expected Java 21 for the UE5.6 Android toolchain.')
    if not (Path(java).resolve().parent / 'javac').is_file():
        parser.error('A Java 21 JDK compiler is required, not only the runtime. Run setup_java.py and configure JAVA_HOME.')
    sdk = args.sdk.expanduser().resolve()
    sdk.mkdir(parents=True, exist_ok=True)
    manager = sdk / 'cmdline-tools/13.0/bin/sdkmanager'
    bootstrap_receipt = manager.parent.parent / 'aetheria-source.json'
    bootstrap = None
    if not manager.is_file():
        print('Reading official Google SDK repository metadata.', flush=True)
        with urllib.request.urlopen(BASE + 'repository2-1.xml', timeout=60) as response:
            root = ET.fromstring(response.read())
        candidates = []
        for package in root.iter('remotePackage'):
            if package.get('path') != CLI_PACKAGE:
                continue
            for archive in package.iter('archive'):
                complete = archive.find('complete')
                if archive.findtext('host-os') == 'linux' and complete is not None:
                    candidates.append(complete)
        if len(candidates) != 1:
            raise RuntimeError('Expected one Linux command-line-tools archive in official metadata.')
        complete = candidates[0]
        relative = complete.findtext('url')
        if not relative or '/' in relative or not relative.endswith('.zip'):
            raise RuntimeError('Unexpected SDK archive URL.')
        expected_hash, expected_size = complete.findtext('checksum'), int(complete.findtext('size'))
        bootstrap = {'package': CLI_PACKAGE, 'url': BASE + relative, 'official_sha1': expected_hash, 'bytes': expected_size}
        print(f'Downloading official command-line tools ({expected_size // 1048576} MiB).', flush=True)
        with tempfile.TemporaryDirectory(dir=sdk, prefix='android-bootstrap-') as directory:
            download = Path(directory) / 'tools.zip'
            digest, sha256, received = hashlib.sha1(), hashlib.sha256(), 0
            with urllib.request.urlopen(BASE + relative, timeout=90) as response, download.open('wb') as target:
                while chunk := response.read(1048576):
                    digest.update(chunk)
                    sha256.update(chunk)
                    received += len(chunk)
                    target.write(chunk)
            if received != expected_size or digest.hexdigest() != expected_hash:
                raise RuntimeError('Official SDK archive size/checksum mismatch. Nothing extracted.')
            bootstrap['sha256'] = sha256.hexdigest()
            with zipfile.ZipFile(download) as archive:
                for entry in archive.infolist():
                    parts = Path(entry.filename).parts
                    if not parts or parts[0] != 'cmdline-tools' or '..' in parts or Path(entry.filename).is_absolute():
                        raise RuntimeError('Unsafe archive path.')
                archive.extractall(directory)
            source = Path(directory) / 'cmdline-tools'
            (sdk / 'cmdline-tools').mkdir(exist_ok=True)
            if manager.parent.parent.exists():
                raise RuntimeError('Partial cmdline-tools/13.0 installation exists; inspect it before retrying.')
            shutil.move(str(source), str(manager.parent.parent))
            for executable in manager.parent.iterdir():
                if executable.is_file():
                    executable.chmod(executable.stat().st_mode | 0o111)
        bootstrap_receipt.write_text(json.dumps(bootstrap, indent=2) + '\n')
    elif bootstrap_receipt.is_file():
        bootstrap = json.loads(bootstrap_receipt.read_text())
    if args.bootstrap_only:
        print(json.dumps({'sdkmanager': str(manager), 'bootstrap_archive': bootstrap, 'package_install': 'not_run'}))
        return
    print('Installing pinned UE5.6.1 Android packages using sdkmanager.', flush=True)
    env = dict(os.environ, ANDROID_HOME=str(sdk), ANDROID_SDK_ROOT=str(sdk))
    # These answers apply only to the explicit package list above.
    run([manager, f'--sdk_root={sdk}', '--install', *PACKAGES], input='y\n' * 32, text=True, env=env)
    ndk = sdk / 'ndk/27.2.12479018'
    required = [sdk / 'platforms/android-34/android.jar', sdk / 'build-tools/35.0.1/aapt2', sdk / 'build-tools/35.0.1/apksigner', sdk / 'platform-tools/adb', ndk / 'source.properties', ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/clang', sdk / 'cmake/3.22.1/bin/cmake']
    if any(not path.is_file() for path in required):
        raise RuntimeError('sdkmanager returned but required package files are missing.')
    probes = {}
    for name, command in {
        'adb': [sdk / 'platform-tools/adb', 'version'],
        'aapt2': [sdk / 'build-tools/35.0.1/aapt2', 'version'],
        'apksigner': [sdk / 'build-tools/35.0.1/apksigner', 'version'],
        'cmake': [sdk / 'cmake/3.22.1/bin/cmake', '--version'],
        'clang': [ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/clang', '--version'],
    }.items():
        result = run(command, capture_output=True, text=True, env=env)
        probes[name] = (result.stdout + result.stderr).strip()
    receipt = {'sdk': str(sdk), 'ndk': str(ndk), 'packages': [CLI_PACKAGE, *PACKAGES], 'java': java_version.stderr.strip(), 'probes': probes, 'bootstrap_archive': bootstrap, 'unreal_engine': 'not_installed_by_this_script', 'apk': 'not_built'}
    (sdk / 'aetheria-toolchain.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
