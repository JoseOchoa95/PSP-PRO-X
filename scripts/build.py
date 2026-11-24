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


def configure_native_mode(args):
    """Normal PRO-X builds use the real native flow; previews stay opt-in."""
    selected=sum(bool(getattr(args,name,False)) for name in
                 ('native_query','native_offer_preview','native_progress_preview','native_ota','native_install_preview',
                  'resident_query','resident_dedicated_preview','resident_dedicated_ota'))
    if selected>1 or (selected and args.mode!='pro-x'):
        raise ValueError('select one native mode for pro-x only')
    if args.mode=='pro-x' and not selected:
        args.native_ota=True
    if getattr(args,'resident_dedicated_ota',False):
        args.native_ota=True
        args.resident_query=True
    if getattr(args,'native_install_preview',False): args.native_ota=True
    if args.native_ota: args.native_query=True
    if args.native_progress_preview: args.native_offer_preview=True
    if args.native_offer_preview: args.native_query=True
    if getattr(args,'resident_dedicated_preview',False): args.resident_query=True
    if getattr(args,'resident_query',False): args.native_query=True
    args.native_background=args.native_ota and not getattr(args,'native_install_preview',False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['reference', 'pro-x'])
    parser.add_argument('--native-query',action='store_true',help='real catalog query in original UI; native installation blocked')
    parser.add_argument('--resident-query',action='store_true',help='opt-in model 1 resident flash0 catalog query diagnostic')
    parser.add_argument('--resident-dedicated-preview',action='store_true',
                        help='resident dedicated transfer test; does not install firmware')
    parser.add_argument('--resident-dedicated-ota',action='store_true',
                        help='local candidate for real resident dedicated EBOOT updates')
    parser.add_argument('--native-offer-preview',action='store_true',help='simulated positive native UI after real query; all native writes blocked')
    parser.add_argument('--native-progress-preview',action='store_true',help='synthetic incomplete progress through a virtual file; no real update writes')
    parser.add_argument('--native-ota',action='store_true',help='real native download/staging and background installation (the PRO-X default)')
    parser.add_argument('--native-install-preview',action='store_true',help='NO-FLASH synthetic original-UI background module/restart diagnostic; no Internet update')
    args = parser.parse_args()
    try: configure_native_mode(args)
    except ValueError as error: parser.error(str(error))
    if args.mode == 'pro-x':
        run(__import__('sys').executable, 'scripts/generate-product.py', '--check')
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
    if args.native_query:
        flags='-DPROX_NATIVE_UI_QUERY=1'
        if args.native_offer_preview: flags+=' -DPROX_NATIVE_OFFER_PREVIEW=1'
        if args.native_progress_preview: flags+=' -DPROX_NATIVE_PROGRESS_PREVIEW=1'
        if args.native_ota: flags+=' -DPROX_NATIVE_OTA=1'
        if args.native_install_preview: flags+=' -DPROX_NATIVE_INSTALL_PREVIEW=1'
        if args.native_background: flags+=' -DPROX_NATIVE_BACKGROUND=1'
        if args.resident_query: flags+=' -DPROX_NATIVE_RESIDENT_QUERY=1'
        if args.resident_dedicated_preview: flags+=' -DPROX_NATIVE_OTA=1 -DPROX_NATIVE_DEDICATED_PREVIEW=1'
        if args.resident_dedicated_ota: flags+=' -DPROX_NATIVE_DEDICATED_OTA=1'
        command[command.index(image):command.index(image)]=['--env','PROX_NATIVE_UPDATE_FLAGS='+flags]
    with log.open('wb') as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    print(log.relative_to(ROOT), flush=True)
    if result.returncode:
        print(log.read_text(errors='replace')[-6500:])
        raise SystemExit(result.returncode)
    metadata = {
        'mode': args.mode, 'upstream_revision': UPSTREAM,
        'native_query_only':args.native_query and not args.native_ota,
        'native_offer_preview':args.native_offer_preview,
        'native_progress_preview':args.native_progress_preview,
        'native_ota':args.native_ota,
        'native_install_preview':args.native_install_preview,
        'native_background':args.native_background,
        'resident_query':args.resident_query,
        'resident_dedicated_preview':args.resident_dedicated_preview,
        'resident_dedicated_ota':args.resident_dedicated_ota,
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
    snapshot=ROOT/'source-snapshot.json'
    if snapshot.exists():
        origin=json.loads(snapshot.read_text())
        if origin.get('test_only'):
            if origin.get('temporary_test_product_version')!=VERSION:
                raise RuntimeError('Temporary test identity differs from generated product')
            metadata.update(test_only=True,feature_product_version=origin['feature_product_version'],
                            temporary_test_product_version=VERSION)
    (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    latest = ROOT / 'artifacts' / (args.mode + '-build.json')
    latest.write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print('PASS', args.mode, metadata['source_tree_sha256'])


if __name__ == '__main__':
    main()
