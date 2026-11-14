"""Build/package only the standalone PSP network diagnostic, never the CFW."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
IMAGE = 'pro-x-toolchain:phase1'
TOOL_VERSION = '1.0'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    identifier = 'netdiag-' + uuid.uuid4().hex[:12]
    source = ROOT / 'firmware/tools/netdiag'
    names = ['Makefile', 'README.md', 'main.c', 'protocol.c', 'protocol.h']
    inputs = {name: sha((source / name).read_bytes()) for name in names}
    work = ROOT / 'artifacts/work' / identifier
    work.mkdir(parents=True)
    for name in inputs:
        shutil.copy2(source / name, work / name)
    log = ROOT / 'artifacts/logs' / (identifier + '.log')
    log.parent.mkdir(parents=True, exist_ok=True)
    image = json.loads(subprocess.check_output(['docker', 'image', 'inspect', IMAGE], cwd=ROOT))[0]['Id']
    command = ['docker', 'run', '--rm', '--network', 'none', '--mount',
               'type=bind,source=' + str(work) + ',target=/src', image,
               'make', '-C', '/src', 'BUILD_ID=' + identifier]
    with log.open('wb') as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    print('Build log:', log.relative_to(ROOT), flush=True)
    if result.returncode:
        print(log.read_text(errors='replace')[-9000:])
        raise SystemExit(result.returncode)
    binary = (work / 'EBOOT.PBP').read_bytes()
    if binary[:4] != b'\x00PBP':
        raise RuntimeError('Not a PSP executable')
    destination = ROOT / 'builds/PSP/GAME/NETDIAG'
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'EBOOT.PBP').write_bytes(binary)
    # Preserve earlier delivered diagnostic ZIPs when iterating on a report.
    stem = 'pro-x-netdiag-' + TOOL_VERSION + '-661-' + identifier[8:]
    head = (subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
            if (ROOT / '.git').exists() else
            json.loads((ROOT / 'source-snapshot.json').read_text())['repository_head'])
    manifest = {
        'tool': 'PRO-X Network Diagnostic', 'tool_version': TOOL_VERSION,
        'build_id': identifier, 'firmware_target': '6.61',
        'status': 'diagnostic-only; physical execution NOT RUN',
        'firmware_changed': False, 'product_version_changed': False,
        'repository_head': head,
        'source_identity': 'packaged working-tree files and hashes; HEAD alone is not the diagnostic source identity',
        'source_files': inputs, 'build_script_sha256': sha(Path(__file__).read_bytes()),
        'toolchain_image': image, 'command': command,
        'eboot_sha256': sha(binary),
    }
    readme = (source / 'README.md').read_bytes()
    manifest_bytes = (json.dumps(manifest, indent=2) + '\n').encode()
    (destination / 'README.md').write_bytes(readme)
    (destination / 'build-manifest.json').write_bytes(manifest_bytes)
    archive = ROOT / 'builds' / (stem + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('PSP/GAME/NETDIAG/EBOOT.PBP', binary)
        z.writestr('PSP/GAME/NETDIAG/README.md', readme)
        z.writestr('PSP/GAME/NETDIAG/build-manifest.json', manifest_bytes)
        z.write(ROOT / 'LICENSE', 'LICENSE')
        for name in inputs:
            z.write(source / name, 'source/firmware/tools/netdiag/' + name)
        z.write(Path(__file__), 'source/scripts/build-netdiag.py')
        z.write(ROOT / 'LICENSE', 'source/LICENSE')
        z.write(ROOT / 'tests/native/test_netdiag_protocol.c', 'source/tests/native/test_netdiag_protocol.c')
        z.writestr('source/source-snapshot.json', json.dumps({'repository_head': head}) + '\n')
    hashes = sha(archive.read_bytes()) + '  ' + archive.name + '\n'
    hashes += sha(binary) + '  PSP/GAME/NETDIAG/EBOOT.PBP\n'
    (ROOT / 'builds' / (stem + '.sha256')).write_text(hashes, encoding='ascii')
    (ROOT / 'builds' / (stem + '-manifest.json')).write_bytes(manifest_bytes)
    print(json.dumps({'archive': str(archive), 'build_id': identifier,
                      'eboot_sha256': sha(binary)}, indent=2))


if __name__ == '__main__':
    main()
