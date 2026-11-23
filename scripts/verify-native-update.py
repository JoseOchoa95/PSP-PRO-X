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
from test_update_contract import ContractTests, NAMES

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',type=Path)
    parser.add_argument('--running-version',default='0.2.0',help='Older source version for the isolated native fixture')
    args=parser.parse_args()
    ContractTests.setUpClass()
    work=ROOT/'artifacts/phase3/contract-native'
    raw=(ROOT/'builds/pro-x-update.json').read_bytes()
    contract=json.loads(raw)
    build=json.loads((ROOT/'builds'/('pro-x-'+contract['version']+'-661-build.json')).read_bytes())
    # Unlike the synthetic 0.4.0 installer fixtures, compile this consumer with
    # the actual generated product header and verify the real delivered files.
    tls=ROOT/'artifacts/phase3/https-native/mbedtls'
    installer=work/'produced-installer-test'
    command=['cc','-std=c99','-Wall','-Wextra','-Werror','-DPROX_OTA_NATIVE','-DPROX_OTA_FAULT_TEST',
             '-DMBEDTLS_CONFIG_FILE="prox_mbedtls_config.h"','-I'+str(ROOT/'firmware/Common'),
             '-I'+str(ROOT/'tests/native'),'-I'+str(ROOT/'firmware/Updater'),'-I'+str(tls/'include'),
             str(ROOT/'tests/native/test_ota_installer.c')]
    command += [str(ROOT/'firmware/Updater'/n) for n in ('json.c','contract.c','policy.c','stage.c','ota_installer.c')]
    command += ['-L'+str(tls/'library'),'-lmbedcrypto','-lz','-o',str(installer)]
    subprocess.run(command,check=True,capture_output=True)
    base='https://github.com/JoseOchoa95/PSP-PRO-X/releases/download/'+contract['tag']+'/'
    release={'draft':False,'prerelease':False,'tag_name':contract['tag'],
             'name':'ISOLATED GENERATED-ASSET VALIDATION; NOT A PUBLISHED RELEASE',
             'assets':[{'name':'pro-x-update.json','size':len(raw),'digest':'sha256:'+digest(raw),
                        'browser_download_url':base+'pro-x-update.json'},
                       {**contract['package_asset'],'digest':'sha256:'+contract['package_asset']['sha256'],
                        'browser_download_url':base+contract['package_asset']['name']}]}
    with tempfile.TemporaryDirectory(dir=work) as directory:
        d=Path(directory); (d/'stage').mkdir(); (d/'release.json').write_text(json.dumps(release))
        env={**os.environ,'PROX_FIXTURE_RUNNING_VERSION':args.running_version}
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
        game=d/'ms0:/PSP/GAME'; stage=game/'PROXUPDATE'/('.stage-'+contract['build_id']+'-00000001')
        stage.mkdir(parents=True)
        expected={}
        for i,name in enumerate(NAMES):
            data=(d/'stage'/name).read_bytes()
            staged=stage/name; staged.parent.mkdir(parents=True,exist_ok=True); staged.write_bytes(data)
            target=game/name if i < 4 else game/'PROXUPDATE'/name
            target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(b'old fixture file')
            expected[target]=data
        markers={d/'ms0:/seplugins/vsh.txt':b'preserve fixture plugin list',
                 game/'PROXUPDATE/report-previous.txt':b'preserve fixture report'}
        for path,data in markers.items(): path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
        (stage/'pro-x-update.json').write_bytes(raw)
        path='ms0:/PSP/GAME/PROXUPDATE/'+stage.name+'/PROUPDATE/installer.prx'
        installed=subprocess.run([str(installer),path,digest(raw),args.running_version,'ok','commit'],
                                 cwd=d,capture_output=True,text=True,timeout=30)
        (ROOT/'artifacts/phase3/produced-installer-native.log').write_text(installed.stdout+installed.stderr)
        if installed.returncode or 'NO FLASH INSTALLATION CAPABILITY' not in installed.stdout:
            raise RuntimeError('Actual-version installer preflight/storage fixture failed: '+installed.stdout+installed.stderr)
        assert all(path.read_bytes()==data for path,data in expected.items())
        assert all(path.read_bytes()==data for path,data in markers.items())
        assert not list(game.rglob('*.prox-new')) and not list(game.rglob('*.prox-old'))
        assert (game/'PROXUPDATE/installed.pending').read_text()==('PROX-OTA-1\n'+contract['version']+'\n'+contract['build_id']+'\ncommitted\n')
    summary={'status':'PASS','build_id':build['build_id'],'fixture_running_version':args.running_version,
             'produced_assets':'Production native parser verified and staged all eight generated payload files, including real PBP/SFO/executable headers',
             'produced_installer':'PASS: actual product header, real eight payloads, preflight/storage commit and preserved plugin/report markers in an isolated fixture; NO FLASH INSTALLATION',
             'fixture_limit':'Locally constructed release/older source parameter; no PSP loader or flash capability; not an official stable update'}
    actual=json.loads((ROOT/'artifacts/pro-x-build.json').read_text())
    if actual.get('native_background'):
        if actual['build_id']!=build['build_id']:raise RuntimeError('Signed consumer compiled build differs')
        signed=ROOT/'artifacts/phase3/signed-native';signed.mkdir(exist_ok=True)
        from native_signed_templates import render
        (signed/'native_signed_templates.h').write_bytes(render((ROOT/'firmware/contrib/PrxEncrypter/main.c').read_bytes()))
        executable=signed/'signed-package-test'
        command=['cc','-std=c99','-O2','-I'+str(ROOT/'firmware/Common'),'-I'+str(ROOT/'firmware/Updater'),'-I'+str(signed),
                 str(ROOT/'tests/native/test_native_signed_package.c')]
        command += [str(ROOT/'firmware/Updater'/n) for n in ('native_signed.c','native_payload.c','native_kirk.c','native_crypto.c')]
        command += ['-lz','-o',str(executable)]
        subprocess.run(command,check=True,capture_output=True)
        names=['SystemControl/systemctrl.prx','Vshctrl/vshctrl.prx','ISODrivers/Galaxy/galaxy.prx','Stargate/stargate.prx',
               'ISODrivers/March33/march33_660.prx','ISODrivers/Inferno/inferno.prx','usbdevice/usbdevice.prx','Popcorn/popcorn.prx',
               'Satelite/satelite.prx','Recovery/recovery.prx','Rebootex/Rebootex.prx']
        with tempfile.TemporaryDirectory(dir=signed) as temporary:
            output=Path(temporary)
            result=subprocess.run([str(executable),str(ROOT/actual['output']/'PSP/GAME/PROUPDATE/EBOOT.PBP'),str(output)],
                                  capture_output=True,text=True,timeout=30)
            (ROOT/'artifacts/phase3/produced-signed-native.log').write_text(result.stdout+result.stderr)
            if result.returncode:raise RuntimeError('Actual signed package rejected by data decoder: '+result.stdout+result.stderr)
            decoded={}
            for i,name in enumerate(names):
                part=(output/('%02u.prx'%i)).read_bytes()
                if part!=(ROOT/actual['output']/'build-tree'/name).read_bytes():raise RuntimeError('Decoded target module differs: '+name)
                decoded[name]={'size':len(part),'sha256':digest(part)}
            if len(list(output.iterdir()))!=11:raise RuntimeError('Unexpected signed decode output')
        summary.update(signed_payload_consumer='PASS: actual signed produced PBP, checked CMAC/decrypt/GZIP/ELF and eleven exact compiled target images; no PSP module loading/flash',
                       decoded_target_parts=decoded)
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
