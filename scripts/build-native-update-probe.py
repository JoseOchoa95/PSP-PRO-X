"""Build/package a clearly separate firmware diagnostic for the original XMB UI."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys,uuid
from build import ROOT,inventory,digest,sha,repository_head,IMAGE,UPSTREAM
from verify import verify_source,verify_build,require
from package import archive
from product import VERSION
from native_ui_probe_package import package_probe

def main():
    subprocess.run([sys.executable,'scripts/generate-product.py','--check'],cwd=ROOT,check=True)
    reference=json.loads((ROOT/'artifacts/reference-build.json').read_text())
    verify_source(reference)
    identifier='pro-x-'+uuid.uuid4().hex[:12]
    source=ROOT/'.phase1/inputs'/identifier
    shutil.copytree(ROOT/'firmware',source,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
    inputs=inventory(source)
    output=ROOT/'artifacts/work'/identifier
    log=ROOT/'artifacts/logs'/(identifier+'.log')
    image=json.loads(subprocess.check_output(['docker','image','inspect',IMAGE],cwd=ROOT))[0]['Id']
    command=['docker','run','--rm','--network','none','--env','PROX_NATIVE_UPDATE_FLAGS=-DPROX_NATIVE_UI_PROBE=1',
             '--mount',f'type=bind,source={ROOT},target=/work',image,'sh','/work/scripts/compile.sh',
             '/work/'+source.relative_to(ROOT).as_posix(),'/work/'+output.relative_to(ROOT).as_posix()]
    print('Building original native UI diagnostic:',identifier,flush=True)
    with log.open('wb') as stream:
        result=subprocess.run(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(errors='replace')[-5000:]); raise SystemExit(result.returncode)
    metadata={'mode':'pro-x','kind':'original-native-ui-diagnostic','production_ota':False,
              'simulated_catalog_result':True,'query_downloads':False,'hardware_validated':False,
              'product_version':VERSION,'build_id':identifier,'repository_head':repository_head(),
              'upstream_revision':UPSTREAM,'source_files':inputs,'source_tree_sha256':digest(inputs),
              'toolchain_image':image,'command':command,'output':output.relative_to(ROOT).as_posix(),
              'log':log.relative_to(ROOT).as_posix(),'compile_script_sha256':sha((ROOT/'scripts/compile.sh').read_bytes()),
              'dockerfile_sha256':sha((ROOT/'scripts/Dockerfile').read_bytes()),
              'artifacts':inventory(output/'PSP'),'modules':inventory(output/'modules'),
              'diagnostics':inventory(output/'diagnostics'),'ui_preview':inventory(output/'ui-preview')}
    (output/'build.json').write_text(json.dumps(metadata,indent=2)+'\n')
    checked=verify_build(metadata)
    elf=(output/'build-tree/Vshctrl/vshctrl.elf').read_bytes()
    require(b'native probe SIMULATED current' in elf,
            'Native diagnostic provider not linked')
    require(b'ProxNativeUiProbeForUser' in elf,'Diagnostic syscalls are not exported')
    for symbol in ('InitEx','Setting','Connect','Read','Abort','End'):
        nid=hashlib.sha1(('proxNativeProbe'+symbol).encode()).digest()[:4]
        require(nid in elf,'Missing diagnostic syscall NID: '+symbol)
    require(inventory(ROOT/'firmware')==inputs,'Firmware changed during compilation')
    (ROOT/'artifacts/native-ui-probe-build.json').write_text(json.dumps(metadata,indent=2)+'\n')
    package_probe(metadata)
    print('PASS: compiled/verified original UI diagnostic; PSP test NOT RUN')

if __name__=='__main__': main()
