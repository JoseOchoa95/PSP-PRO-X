"""Verify generated schema-1 assets with the production C parser (offline).

Run inside the pinned Docker image after packaging. The fake release and older
source-version parameter are container-validation fixtures, never published.
There is no PSP loader or flash capability. --catalog optionally checks a saved
public API response with the production orchestrator's native test adapters.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tests'))
from test_update_contract import ContractTests

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',type=Path)
    args=parser.parse_args()
    ContractTests.setUpClass()
    work=ROOT/'artifacts/phase3/contract-native'
    raw=(ROOT/'builds/pro-x-update.json').read_bytes()
    contract=json.loads(raw)
    build=json.loads((ROOT/'builds'/('pro-x-'+contract['version']+'-661-build.json')).read_bytes())
    base='https://github.com/JoseOchoa95/PSP-PRO-X/releases/download/'+contract['tag']+'/'
    release={'draft':False,'prerelease':False,'tag_name':contract['tag'],
             'name':'ISOLATED GENERATED-ASSET VALIDATION; NOT A PUBLISHED RELEASE',
             'assets':[{'name':'pro-x-update.json','size':len(raw),'digest':'sha256:'+digest(raw),
                        'browser_download_url':base+'pro-x-update.json'},
                       {**contract['package_asset'],'digest':'sha256:'+contract['package_asset']['sha256'],
                        'browser_download_url':base+contract['package_asset']['name']}]}
    with tempfile.TemporaryDirectory(dir=work) as directory:
        d=Path(directory); (d/'stage').mkdir(); (d/'release.json').write_text(json.dumps(release))
        env={**os.environ,'PROX_FIXTURE_RUNNING_VERSION':'0.2.0'}
        result=subprocess.run([str(work/'contract-test'),'stage',str(d/'release.json'),
                               str(ROOT/'builds/pro-x-update.json'),str(ROOT/'builds'/contract['package_asset']['name']),
                               str(d/'stage')],env=env,capture_output=True,text=True,timeout=30)
        (ROOT/'artifacts/phase3/produced-update-native.log').write_text(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError('Generated assets rejected by production C parser: '+result.stdout+result.stderr)
        for f in contract['files']:
            data=(d/'stage'/f['path']).read_bytes()
            if len(data) != f['size'] or digest(data) != f['sha256']:
                raise RuntimeError('Extracted file mismatch: '+f['path'])
        if len([p for p in (d/'stage').rglob('*') if p.is_file()]) != 8:
            raise RuntimeError('Unexpected generated update inventory')
    summary={'status':'PASS','build_id':build['build_id'],
             'produced_assets':'Production native parser verified and staged all eight generated payload files, including real PBP/SFO/executable headers',
             'fixture_limit':'Locally constructed release/older source parameter; no PSP loader or flash capability; not an official stable update'}
    if args.catalog:
        catalog=args.catalog.resolve()
        with tempfile.TemporaryDirectory(dir=work) as directory:
            d=Path(directory); (d/'client').mkdir(); (d/'catalog.json').write_bytes(catalog.read_bytes())
            result=subprocess.run([str(work/'flow-test'),str(d),str(d/'client'),'ok'],capture_output=True,text=True,timeout=30)
            (ROOT/'artifacts/phase3/public-catalog-native.log').write_text(result.stdout+result.stderr)
            if result.returncode != 2 or (d/'client/stage').exists():
                raise RuntimeError('Saved current catalog result differs: '+result.stdout+result.stderr)
        summary.update(public_catalog='Saved public API response: no eligible newer update; no stage/installer',
                       catalog_sha256=digest(catalog.read_bytes()),
                       releases=[{'tag':r['tag_name'],'draft':r['draft'],'prerelease':r['prerelease']} for r in json.loads(catalog.read_bytes())])
    (ROOT/'artifacts/phase3/produced-update-native.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__ == '__main__':
    main()
