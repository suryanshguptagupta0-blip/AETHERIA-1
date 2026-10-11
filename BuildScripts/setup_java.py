#!/usr/bin/env python3
"""Install free Temurin OpenJDK 21 from Adoptium's official GitHub release.

Linux x86_64 only. Verifies the official SHA256 before extracting; does not
install Unreal Engine or create an APK. The JDK includes its GPL/Classpath notices.
"""
import argparse
import hashlib
import json
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
from pathlib import Path

API = 'https://api.github.com/repos/adoptium/temurin21-binaries/releases/latest'
PREFIX = 'https://github.com/adoptium/temurin21-binaries/releases/download/'


def request(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'Aetheria-Development'}), timeout=90)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dest', type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
        parser.error('This bootstrap supports Linux x86_64. Use the official JDK for another build host.')
    destination = args.dest.expanduser().resolve()
    receipt_path = destination / 'aetheria-source.json'
    if not (destination / 'bin/javac').is_file():
        if destination.exists() and any(destination.iterdir()):
            parser.error('Destination is not an installed JDK and is not empty; inspect it before retrying.')
        destination.parent.mkdir(parents=True, exist_ok=True)
        print('Reading official Adoptium OpenJDK 21 release metadata.', flush=True)
        with request(API) as response:
            release = json.load(response)
        candidates = [asset for asset in release['assets'] if re.fullmatch(r'OpenJDK21U-jdk_x64_linux_hotspot_[^/]+\.tar\.gz', asset['name'])]
        if len(candidates) != 1:
            raise RuntimeError('Expected exactly one official Linux x64 JDK 21 archive.')
        asset = candidates[0]
        url, digest = asset['browser_download_url'], asset.get('digest', '')
        if not url.startswith(PREFIX) or not re.fullmatch(r'sha256:[0-9a-f]{64}', digest):
            raise RuntimeError('Official release URL or SHA256 metadata is unavailable.')
        print(f'Downloading free OpenJDK {release["tag_name"]} ({asset["size"] // 1048576} MiB).', flush=True)
        with tempfile.TemporaryDirectory(dir=destination.parent, prefix='jdk-bootstrap-') as temporary:
            temporary = Path(temporary)
            archive_path = temporary / 'jdk.tar.gz'
            sha, size = hashlib.sha256(), 0
            with request(url) as response, archive_path.open('wb') as output:
                while chunk := response.read(1048576):
                    sha.update(chunk)
                    size += len(chunk)
                    output.write(chunk)
            if size != asset['size'] or sha.hexdigest() != digest.removeprefix('sha256:'):
                raise RuntimeError('Official JDK archive size/SHA256 mismatch. Nothing extracted.')
            with tarfile.open(archive_path, 'r:gz') as archive:
                roots = {Path(entry.name).parts[0] for entry in archive.getmembers() if Path(entry.name).parts}
                if len(roots) != 1:
                    raise RuntimeError('Unexpected JDK archive layout.')
                archive.extractall(temporary, filter='data')
            unpacked = temporary / roots.pop()
            if not (unpacked / 'bin/javac').is_file():
                raise RuntimeError('Archive did not contain a JDK compiler.')
            if destination.exists():
                destination.rmdir()
            shutil.move(str(unpacked), str(destination))
        receipt = {'release': release['tag_name'], 'official_metadata': API, 'archive': url, 'sha256': sha.hexdigest(), 'bytes': size, 'license': 'GPL-2.0 with Classpath Exception; see legal/ in the JDK'}
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    versions = {}
    for name in ('java', 'javac'):
        result = subprocess.run([str(destination / 'bin' / name), '-version'], check=True, text=True, capture_output=True)
        versions[name] = (result.stdout + result.stderr).strip()
    if not re.search(r'version "21\.', versions['java']) or not versions['javac'].startswith('javac 21.'):
        raise RuntimeError('Installed Java runtime/compiler are not both version 21.')
    print(json.dumps({'java_home': str(destination), 'versions': versions, 'source': json.loads(receipt_path.read_text()) if receipt_path.is_file() else 'existing_JDK_not_downloaded_by_this_script', 'native_build': 'not_run'}, indent=2))


if __name__ == '__main__':
    main()
