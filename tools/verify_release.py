"""Check release hashes, metadata, installation ZIP and optional tag/version match."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from build import SDK_COMMIT, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / 'dist')
    parser.add_argument('--tag')
    args = parser.parse_args()
    directory = args.directory.resolve()
    version = (ROOT / 'VERSION').read_text(encoding='ascii').strip()
    if args.tag and args.tag != 'v' + version:
        raise SystemExit('Release tag does not match VERSION')
    for name in ('H1Mota.bda', 'H1Mota-install.zip'):
        actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        expected = (directory / (name + '.sha256')).read_text(encoding='ascii')
        if expected != actual + '  ' + name + '\n':
            raise SystemExit('SHA-256 mismatch: ' + name)
    metadata = json.loads((directory / 'H1Mota.build.json').read_text(encoding='utf8'))
    bda = (directory / 'H1Mota.bda').read_bytes()
    if (metadata['version'] != version or metadata['sdk_commit'] != SDK_COMMIT
            or metadata['sha256'] != hashlib.sha256(bda).hexdigest()
            or metadata['bytes'] != len(bda)):
        raise SystemExit('Build metadata does not match release')
    evidence = json.loads((ROOT / 'docs/firmware-verification.json').read_text(encoding='utf8'))
    if (evidence['result'] != 'passed' or evidence['bda_sha256'] != metadata['sha256']
            or evidence['bda_bytes'] != len(bda) or evidence['version'] != version
            or evidence['sdk_commit'] != SDK_COMMIT):
        raise SystemExit('Release does not match the recorded firmware-tested BDA')
    sources = {'src/' + name: digest for name, digest in evidence['sources_sha256'].items()}
    actual_sources = {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in sorted((ROOT / 'src').glob('*')) if p.is_file()}
    if sources != metadata['sources'] or sources != actual_sources:
        raise SystemExit('Release sources do not match the firmware-tested sources')
    for name, info in evidence['screenshots'].items():
        if hashlib.sha256((ROOT / 'docs/screenshots' / name).read_bytes()).hexdigest() != info['sha256']:
            raise SystemExit('Firmware screenshot changed: ' + name)
    with zipfile.ZipFile(directory / 'H1Mota-install.zip') as package:
        if package.testzip() is not None:
            raise SystemExit('Installation ZIP failed CRC verification')
        if package.read('应用/程序/H1Mota.bda') != bda:
            raise SystemExit('Installation ZIP contains a different BDA')
        if package.read('H1Mota.build.json') != (directory / 'H1Mota.build.json').read_bytes():
            raise SystemExit('Installation ZIP contains different metadata')
        if package.read('VERSION').decode('ascii').strip() != version:
            raise SystemExit('Installation ZIP contains a different version')
        if package.read('docs/firmware-verification.json') != (ROOT / 'docs/firmware-verification.json').read_bytes():
            raise SystemExit('Installation ZIP contains different firmware evidence')
    print(f'PASS release {version}: {metadata["sha256"]}')


if __name__ == '__main__':
    main()
