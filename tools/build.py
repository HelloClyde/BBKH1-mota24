"""Build and package the native H1/Y100 V1.41 port with the pinned SDK."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SDK_COMMIT = '067fe072477861dfc8949d7b1a55279fb92d2548'
SOURCE_DATE_EPOCH = 1792713600


def git(sdk, *args):
    return subprocess.check_output(['git', '-C', str(sdk), *args], text=True).strip()


def checked_sdk(sdk):
    if not (sdk / 'h1_bda/build.py').is_file():
        raise SystemExit('SDK missing: run git submodule update --init sdk')
    commit = git(sdk, 'rev-parse', 'HEAD')
    if commit != SDK_COMMIT:
        raise SystemExit(f'SDK must be pinned to {SDK_COMMIT}; found {commit}')
    if git(sdk, 'status', '--porcelain', '--untracked-files=all'):
        raise SystemExit('SDK contains local changes; use a clean pinned checkout')
    return commit


def package_release(out, epoch):
    files = [(out / 'H1Mota.bda', '应用/程序/H1Mota.bda')]
    for name in ('README.md', 'LICENSE', 'NOTICE', 'SOURCES.md', 'VERSION',
                 'RELEASE-NOTES.md', 'docs/verification.md', 'docs/firmware-verification.json'):
        files.append((ROOT / name, name))
    for name in ('H1Mota.bda.sha256', 'H1Mota.build.json'):
        files.append((out / name, name))
    for path in sorted((ROOT / 'docs/screenshots').glob('*.png')):
        files.append((path, path.relative_to(ROOT).as_posix()))
    package = out / 'H1Mota-install.zip'
    stamp = datetime.fromtimestamp(epoch, timezone.utc).timetuple()[:6]
    # Stored entries avoid zlib-version differences between developer and CI hosts.
    with zipfile.ZipFile(package, 'w', compression=zipfile.ZIP_STORED) as archive:
        for path, name in files:
            info = zipfile.ZipInfo(name, stamp)
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())
    (out / 'H1Mota-install.zip.sha256').write_text(
        hashlib.sha256(package.read_bytes()).hexdigest() + '  H1Mota-install.zip\n',
        encoding='ascii', newline='\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=ROOT / 'sdk')
    parser.add_argument('--toolchain', type=Path,
                        default=Path(os.environ.get('H1_GNU_BIN', ROOT / '.tools/toolchain/bin')))
    args = parser.parse_args()
    sdk, toolchain = args.sdk.resolve(), args.toolchain.resolve()
    sdk_commit = checked_sdk(sdk)
    version = (ROOT / 'VERSION').read_text(encoding='ascii').strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?', version):
        raise SystemExit('VERSION must contain a semantic version')
    suffix = '.exe' if os.name == 'nt' else ''
    compiler = toolchain / ('mipsel-none-elf-gcc' + suffix)
    compiler_version = subprocess.check_output([str(compiler), '-dumpfullversion'], text=True).strip()
    if compiler_version != '15.2.0':
        raise SystemExit(f'Tested GNU MIPS compiler 15.2.0 required; found {compiler_version}')
    os.environ['H1_GNU_BIN'] = str(toolchain)
    os.environ.setdefault('SOURCE_DATE_EPOCH', str(SOURCE_DATE_EPOCH))
    epoch = int(os.environ['SOURCE_DATE_EPOCH'])
    sys.path.insert(0, str(sdk))
    from h1_bda.build import build_bda
    from h1_bda.validate import validate_bda
    from h1_bda.resources import PAYLOAD_OFFSET
    out, build = ROOT / 'dist', ROOT / 'build'
    out.mkdir(exist_ok=True)
    build.mkdir(exist_ok=True)
    elf = build / 'H1Mota.elf'
    data = build_bda(
        [ROOT / 'src/entry.S', ROOT / 'src/mota24.c', ROOT / 'src/runtime.c'], '魔塔24层', 0x48,
        [ROOT / 'src'], debug_elf=elf, icon_png=ROOT / 'assets/mota24-icon.png',
        compiler_flags=['-std=c99', '-Wall', '-Wextra', '-Wno-misleading-indentation',
                        '-Wno-unused-function', '-Wno-unused-variable'],
    )
    bda = out / 'H1Mota.bda'
    bda.write_bytes(data)
    validation = validate_bda(bda)
    if not validation['ok']:
        bda.unlink()
        raise SystemExit(f'BDA validation failed: {validation["errors"]}')
    nm = toolchain / ('mipsel-none-elf-nm' + suffix)
    symbols = {}
    for line in subprocess.check_output([str(nm), '-S', str(elf)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 4:
            symbols[fields[-1]] = (int(fields[0], 16), int(fields[1], 16))
    payload_end = 0x83c00020 + len(data) - PAYLOAD_OFFSET
    stack, stack_size = symbols['mota_stack']
    if (symbols['h1_bda_main'][0] != 0x83c00020 or stack_size != 32768
            or stack < payload_end or stack + stack_size > 0x83ff0000):
        raise SystemExit('BDA entry, payload or private stack boundaries are invalid')
    for name in ('g', 'screen', 'mota_diagnostic'):
        address, size = symbols[name]
        if address + size > payload_end:
            raise SystemExit(f'{name} is not initialized by the loaded BDA payload')
    sha = hashlib.sha256(data).hexdigest()
    (out / 'H1Mota.bda.sha256').write_text(f'{sha}  H1Mota.bda\n', encoding='ascii', newline='\n')
    meta = {
        'title': '魔塔24层', 'target': 'BBK H1/Y100 V1.41', 'category': 0x48,
        'version': version, 'resolution': [480, 272], 'map_tile_pixels': 24,
        'bytes': len(data), 'sha256': sha, 'sdk_commit': sdk_commit,
        'compiler': 'mipsel-none-elf-gcc ' + compiler_version, 'source_date_epoch': epoch,
        'entry': '0x83c00020', 'payload_end': hex(payload_end),
        'private_stack_start': hex(stack), 'private_stack_bytes': stack_size,
        'original_project': 'BBK9588-mota24', 'original_commit': '29df390',
        'save_directory': 'A:\\H1MOTA\\', 'save_format_bytes': 6608,
        'sources': {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted((ROOT / 'src').glob('*')) if p.is_file()},
    }
    (out / 'H1Mota.build.json').write_text(
        json.dumps(meta, ensure_ascii=False, indent=2) + '\n', encoding='utf8', newline='\n')
    package_release(out, epoch)
    print(json.dumps({k: meta[k] for k in ('target', 'bytes', 'sha256')}, ensure_ascii=False))
    print(bda)


if __name__ == '__main__':
    main()
