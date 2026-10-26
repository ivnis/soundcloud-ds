"""Package a tagged-source server bundle and separately built Nintendo DS ROM.

Usage: python tools/package_release.py --rom PATH_TO_CLEAN_BUILD.nds
Only committed files are read; .env, caches, emulator data and builds are excluded.
"""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    if git('status', '--porcelain').strip():
        parser.error('Commit source changes before packaging a release.')
    version = git('show', 'HEAD:VERSION').decode().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        parser.error('VERSION must contain a numeric semantic version.')
    commit = git('rev-parse', 'HEAD').decode().strip()
    rom = args.rom.read_bytes()
    if len(rom) < 512:
        parser.error('ROM is too small for a Nintendo DS header.')
    for header in (0x20, 0x30):
        offset, _, _, size = struct.unpack_from('<4I', rom, header)
        if not size or offset < 512 or offset + size > len(rom):
            parser.error('ROM contains an invalid ARM executable range.')
    entries = {}
    server_files = git('ls-tree', '-r', '--name-only', 'HEAD', 'server').decode().splitlines()
    for path in server_files:
        if path.endswith('.py') or path in ('server/requirements.txt', 'server/.env.example'):
            entries[path] = git('show', f'HEAD:{path}')
    for path in ('LICENSE', 'VERSION', '.env.example', '.dockerignore', 'compose.yaml', 'deploy/Dockerfile'):
        entries[path] = git('show', f'HEAD:{path}')
    for name in ('README.md', 'README.ru.md', 'requirements-windows.txt',
                 'start-server.ps1', 'Start Server.cmd', 'setup-server.ps1', 'Setup Server.cmd'):
        data = git('show', f'HEAD:deploy/server/{name}')
        if name.endswith('.md'):
            data = data.replace(b'(../../LICENSE)', b'(LICENSE)')
        if name.endswith(('.ps1', '.cmd')):
            data = data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        entries[name] = data
    for path in ('Start Cloudflare Tunnel.cmd', 'Stop Cloudflare Tunnel.cmd',
                 'tools/start-cloudflare.ps1', 'tools/stop-cloudflare.ps1'):
        data=git('show',f'HEAD:{path}')
        entries[path]=data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')
    entries['BUILD_INFO.txt'] = (
        f'SoundCloud DS bridge v{version}\nSource commit: {commit}\n'
        f'Source: https://github.com/ivnis/soundcloud-ds/tree/{commit}\n'
    ).encode()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    bundle = output / f'soundcloud-ds-server-v{version}.zip'
    with zipfile.ZipFile(bundle, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    binary = output / f'soundcloud-ds-v{version}.nds'
    binary.write_bytes(rom)
    sums = ''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n'
                   for path in (binary, bundle))
    (output / 'SHA256SUMS.txt').write_text(sums, encoding='ascii', newline='\n')
    for path in (binary, bundle, output / 'SHA256SUMS.txt'):
        print(f'{path.name}: {path.stat().st_size} bytes')


if __name__ == '__main__':
    main()
