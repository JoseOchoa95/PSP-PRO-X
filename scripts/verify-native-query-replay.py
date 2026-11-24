"""Rebuild the query candidate from its companion ZIP and compare PSP bytes."""
import json
import io
import argparse
from pathlib import Path
import subprocess
import zipfile
from build import ROOT, inventory, sha
from product import VERSION
from verify import require


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,help='Explicit companion source ZIP, including a normal distribution archive')
    args=parser.parse_args()
    build = json.loads((ROOT/'artifacts/pro-x-build.json').read_text())
    require(build.get('native_query_only') is True or build.get('native_ota') is True, 'Not a native test build')
    preview=build.get('native_offer_preview',False)
    progress=build.get('native_progress_preview',False)
    ota=build.get('native_ota',False)
    install_preview=build.get('native_install_preview',False)
    background=build.get('native_background',False)
    resident=build.get('resident_query',False)
    dedicated_preview=build.get('resident_dedicated_preview',False)
    dedicated_ota=build.get('resident_dedicated_ota',False)
    source = ROOT/'builds'/('pro-x-'+VERSION+'-661-'+('resident-dedicated-ota' if dedicated_ota else 'resident-dedicated-preview' if dedicated_preview else 'resident-query' if resident else 'native-install-preview' if install_preview else 'native-ota-test' if ota else 'native-progress-preview' if progress else 'native-offer-preview' if preview else 'native-github-query')+'-source.zip')
    if args.source: source=args.source.resolve()
    source_bytes=source.read_bytes(); source_sha=sha(source_bytes)
    base = ROOT/'artifacts/phase3'/('query-replay-'+source_sha[:12])
    base.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(io.BytesIO(source_bytes)) as archive:
        for info in archive.infolist():
            path = Path(info.filename)
            require(not path.is_absolute() and '..' not in path.parts and ':' not in info.filename,
                    'Unsafe source path')
            require((info.external_attr >> 16) & 0o170000 == 0o100000, 'Nonregular source entry')
        archive.extractall(base)
    require(inventory(base/'firmware') == build['source_files'], 'Source archive differs from compiled inputs')
    output = base/'artifacts/work'/build['build_id']
    log = ROOT/'artifacts/logs'/('query-replay-'+build['build_id']+'.log')
    command = ['docker', 'run', '--rm', '--network', 'none', '--mount',
               'type=bind,source='+str(base)+',target=/work', '--env',
               'PROX_NATIVE_UPDATE_FLAGS=-DPROX_NATIVE_UI_QUERY=1'+(' -DPROX_NATIVE_OFFER_PREVIEW=1' if preview else '')+
               (' -DPROX_NATIVE_PROGRESS_PREVIEW=1' if progress else '')+(' -DPROX_NATIVE_OTA=1' if ota else '')+
               (' -DPROX_NATIVE_INSTALL_PREVIEW=1' if install_preview else '')+
               (' -DPROX_NATIVE_BACKGROUND=1' if background else '')+
               (' -DPROX_NATIVE_RESIDENT_QUERY=1' if resident else '')+
               (' -DPROX_NATIVE_DEDICATED_OTA=1' if dedicated_ota else '')+
               (' -DPROX_NATIVE_OTA=1 -DPROX_NATIVE_DEDICATED_PREVIEW=1' if dedicated_preview else ''), build['toolchain_image'],
               'sh', '/work/scripts/compile.sh', '/work/firmware',
               '/work/artifacts/work/'+build['build_id']]
    with log.open('wb') as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    require(not result.returncode, 'Replay failed; see '+str(log))
    require(inventory(output/'PSP') == build['artifacts'], 'Replayed executable or CA bytes differ')
    require(inventory(output/'modules') == build['modules'], 'Replayed embedded module bytes differ')
    if resident:
        local_worker = ROOT/build['output']/'build-tree/Updater/prox_updater.prx'
        require(sha((output/'build-tree/Updater/prox_updater.prx').read_bytes()) == sha(local_worker.read_bytes()),
                'Replayed resident worker differs')
        data_dir = 'dedicated-fixture' if dedicated_preview else 'dedicated-update'
        require(inventory(output/data_dir) == inventory(ROOT/build['output']/data_dir) and
                inventory(output/'diagnostics') == build['diagnostics'],
                'Replayed dedicated EBOOT or diagnostic differs')
    require(sha(source.read_bytes())==source_sha,'Source ZIP changed during replay')
    proof = dict(status='PASS', build_id=build['build_id'], source_zip_sha256=source_sha,
                 artifacts=build['artifacts'], embedded_modules='PASS', network='none')
    if resident:
        proof['resident_worker_sha256'] = sha(local_worker.read_bytes())
        proof['dedicated_update'] = inventory(output/data_dir)
    (ROOT/'artifacts/phase3/native-query-source-replay.json').write_text(json.dumps(proof, indent=2)+'\n')
    print('PASS: source ZIP rebuilt offline; installed executables and embedded modules match')


if __name__ == '__main__':
    main()
