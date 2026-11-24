"""Build and package the standalone flash0 read-only diagnostic."""
import argparse
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
IMAGE = 'pro-x-toolchain:phase1'
TOOL = 'firmware/tools/flashcapacity'
SOURCES = (
    TOOL + '/Makefile',
    TOOL + '/main.c',
    TOOL + '/README.md',
    'firmware/Common/prox_flash_capacity.c',
    'firmware/Common/prox_flash_capacity.h',
    'firmware/include/systemctrl.h',
    'firmware/SystemControl/libs/libpspsystemctrl_kernel/SystemCtrlForKernel.S',
    'tests/native/test_flash_capacity.c',
    'scripts/build-flash-capacity-probe.py',
    'LICENSE',
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def archive(path, files):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as out:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, date_time=(2026, 11, 23, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            out.writestr(entry, data)
    with zipfile.ZipFile(path) as packaged:
        if set(packaged.namelist()) != set(files) or any(packaged.read(name) != data for name, data in files.items()):
            raise RuntimeError('Reopened ZIP differs from inputs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'builds')
    args = parser.parse_args()
    source = {name: (ROOT / name).read_bytes() for name in SOURCES}
    hashes = {name: sha(data) for name, data in source.items()}
    image = subprocess.check_output(['docker', 'image', 'inspect', IMAGE, '--format', '{{.Id}}'],
                                    text=True, cwd=ROOT).strip()
    identifier = 'flashcap-' + sha(json.dumps([image, hashes], sort_keys=True).encode())[:12]
    work = ROOT / 'artifacts/work' / identifier
    work.mkdir(parents=True, exist_ok=False)
    for name, data in source.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    log = ROOT / 'artifacts/logs' / (identifier + '.log')
    log.parent.mkdir(parents=True, exist_ok=True)
    command = ['docker', 'run', '--rm', '--network', 'none', '--mount',
               'type=bind,source=' + str(work) + ',target=/probe', '-w', '/probe', image,
               'sh', '-ec', 'cc -std=c99 -Wall -Wextra -Werror -I firmware/Common '
               'tests/native/test_flash_capacity.c firmware/Common/prox_flash_capacity.c -o /tmp/prox-flash-capacity; '
               '/tmp/prox-flash-capacity; make -C ' + TOOL + ' BUILD_ID=' + identifier]
    with log.open('wb') as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(errors='replace')[-6000:])
        raise SystemExit(result.returncode)

    eboot = (work / TOOL / 'EBOOT.PBP').read_bytes()
    start, end = struct.unpack_from('<2I', eboot, 32)
    if eboot[:4] != b'\x00PBP' or eboot[start:start + 4] != b'\x7fELF' or end != len(eboot):
        raise RuntimeError('Invalid probe PBP')
    required = (identifier.encode(), b'lflash0:0,0', b'ms0:/PSP/GAME/FCAPDIAG/report-')
    if not all(marker in eboot for marker in required):
        raise RuntimeError('Missing probe identity or paths')
    elf = (work / TOOL / 'prox_flash_capacity.elf').read_bytes()
    if any(name in elf for name in (b'sceIoAssign\0', b'sceIoUnassign\0', b'sceIoRemove\0', b'sceIoDevctl\0')):
        raise RuntimeError('Unexpected flash mutation capability')
    if not all(marker in elf for marker in (b'SystemCtrlForKernel\0', b'sctrlKernelExitVSH\0')) or b'sceKernelExitGame\0' in elf:
        raise RuntimeError('Kernel diagnostic must return through the CFW VSH export')

    head = (subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
            if (ROOT / '.git').exists() else
            json.loads((ROOT / 'source-snapshot.json').read_text())['repository_head'])
    manifest = {
        'tool': 'PRO-X Flash0 Capacity',
        'build_id': identifier,
        'firmware_target': '6.61',
        'phase': '3-B / v0.4.0',
        'hardware': 'NOT RUN',
        'firmware_changed': False,
        'source_head': head,
        'inputs': hashes,
        'toolchain_image': image,
        'eboot_sha256': sha(eboot),
    }
    entry = 'PSP/GAME/FCAPDIAG/EBOOT.PBP'
    stem = 'pro-x-0.4.0-661-' + identifier
    destination = args.output.resolve() / identifier
    destination.mkdir(parents=True, exist_ok=False)
    source['source-snapshot.json'] = (json.dumps({'repository_head': head}, indent=2) + '\n').encode()
    source_zip = destination / (stem + '-source.zip')
    archive(source_zip, source)
    manifest['source_zip_sha256'] = sha(source_zip.read_bytes())
    manifest_bytes = (json.dumps(manifest, indent=2) + '\n').encode()
    instructions = source[TOOL + '/README.md']
    binary = destination / (stem + '.zip')
    archive(binary, {entry: eboot, 'README.md': instructions, 'build-manifest.json': manifest_bytes,
                     'LICENSE': source['LICENSE']})
    (destination / entry).parent.mkdir(parents=True, exist_ok=True)
    (destination / entry).write_bytes(eboot)
    (destination / 'README.md').write_bytes(instructions)
    (destination / 'build-manifest.json').write_bytes(manifest_bytes)
    (destination / (stem + '.sha256')).write_text(
        sha(binary.read_bytes()) + '  ' + binary.name + '\n'
        + manifest['source_zip_sha256'] + '  ' + source_zip.name + '\n'
        + sha(manifest_bytes) + '  build-manifest.json\n'
        + manifest['eboot_sha256'] + '  ' + entry + '\n', encoding='ascii')
    print(binary)
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
