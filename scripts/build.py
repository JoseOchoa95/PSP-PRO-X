"""Build isolated 6.61 reference/PRO-X trees; never reuse generated objects."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import tarfile
import uuid
from product import VERSION

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = 'd9435ff4af6ba60466b46019393f6bbd1fc72ac0'
IMAGE = 'pro-x-toolchain:phase1'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inventory(directory):
    return {p.relative_to(directory).as_posix(): sha(p.read_bytes())
            for p in sorted(directory.rglob('*')) if p.is_file() and '__pycache__' not in p.parts}


def digest(files):
    return sha(json.dumps(files, sort_keys=True, separators=(',', ':')).encode())


def run(*args, **kwargs):
    return subprocess.run(args, check=True, cwd=ROOT, **kwargs)


def repository_head():
    if (ROOT / '.git').exists():
        return subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    snapshot = ROOT / 'source-snapshot.json'
    return json.loads(snapshot.read_text()).get('repository_head') if snapshot.exists() else None


def prepare_reference(destination):
    cache = ROOT / '.phase1/upstream-procfw'
    if not (cache / '.git').exists():
        run('git', 'clone', '--no-checkout', 'https://github.com/MrColdbird/procfw.git', str(cache))
    archive = ROOT / '.phase1/reference.tar'
    run('git', '-C', str(cache), 'archive', '--format=tar', '-o', str(archive), UPSTREAM)
    with tarfile.open(archive) as source:
        source.extractall(destination, filter='data')
    expected = json.loads((ROOT / 'docs/upstream-files.json').read_text())
    if inventory(destination) != expected:
        raise RuntimeError('Reference differs from pinned upstream inventory')
    run('git', 'apply', '--whitespace=nowarn', '--directory=' + destination.relative_to(ROOT).as_posix(),
        str(ROOT / 'scripts/build-compat.patch'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['reference', 'pro-x'])
    args = parser.parse_args()
    identifier = args.mode + '-' + uuid.uuid4().hex[:12]
    source = ROOT / '.phase1/inputs' / identifier
    source.mkdir(parents=True)
    if args.mode == 'reference':
        prepare_reference(source)
    else:
        shutil.copytree(ROOT / 'firmware', source, dirs_exist_ok=True, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    inputs = inventory(source)
    output = ROOT / 'artifacts/work' / identifier
    log = ROOT / 'artifacts/logs' / (identifier + '.log')
    log.parent.mkdir(parents=True, exist_ok=True)
    image = json.loads(subprocess.check_output(['docker', 'image', 'inspect', IMAGE], cwd=ROOT))[0]['Id']
    command = ['docker', 'run', '--rm', '--network', 'none', '--mount',
               'type=bind,source=' + str(ROOT) + ',target=/work', image, 'sh',
               '/work/scripts/compile.sh', '/work/' + source.relative_to(ROOT).as_posix(),
               '/work/' + output.relative_to(ROOT).as_posix()]
    with log.open('wb') as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    print(log.relative_to(ROOT), flush=True)
    if result.returncode:
        print(log.read_text(errors='replace')[-6500:])
        raise SystemExit(result.returncode)
    metadata = {
        'mode': args.mode, 'upstream_revision': UPSTREAM,
        'product_version': VERSION if args.mode == 'pro-x' else None,
        'build_id': identifier,
        'repository_head': repository_head(),
        'source_files': inputs, 'source_tree_sha256': digest(inputs),
        'compile_script_sha256': sha((ROOT / 'scripts/compile.sh').read_bytes()),
        'dockerfile_sha256': sha((ROOT / 'scripts/Dockerfile').read_bytes()),
        'toolchain_image': image, 'command': command,
        'output': output.relative_to(ROOT).as_posix(), 'log': log.relative_to(ROOT).as_posix(),
        'artifacts': inventory(output / 'PSP'), 'modules': inventory(output / 'modules'),
        'diagnostics': inventory(output / 'diagnostics'),
        'hardware_validated': False,
    }
    (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    latest = ROOT / 'artifacts' / (args.mode + '-build.json')
    latest.write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print('PASS', args.mode, metadata['source_tree_sha256'])


if __name__ == '__main__':
    main()
