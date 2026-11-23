"""Package the explicit no-flash native background/restart test, never OTA assets."""
from pathlib import Path
import hashlib,json,zipfile
from build import ROOT,inventory,sha
from package import archive
from product import VERSION
from verify import require,verify_source,verify_build,elf_data
import re

def main():
    build=json.loads((ROOT/'artifacts/pro-x-build.json').read_text())
    require(build.get('native_install_preview') is True,'Not the explicit no-flash preview')
    require(build['product_version']==VERSION and inventory(ROOT/'firmware')==build['source_files'],'Source/version changed after build')
    reference=json.loads((ROOT/'artifacts/reference-build.json').read_text())
    require(reference['toolchain_image']==build['toolchain_image'],'Toolchain differs')
    verify_source(reference); checked=verify_build(build)
    output=ROOT/build['output']; tree=output/'build-tree'
    probe=elf_data(output/'PSP/GAME/PROXUPDATE/native-install-preview.prx')
    worker=(tree/'Updater/prox_updater.elf').read_bytes()
    require(b'PROX-NATIVE-INSTALL-PREVIEW-NO-FLASH-V1' in probe,'Missing no-flash module identity')
    require(b'PROX-NATIVE-COMPLETION-PREVIEW-V1' in probe,'Missing native-completion diagnostic identity')
    require(b'PROX-NATIVE-COMPLETION-X-ONLY-V1' in probe,'Missing X-only completion identity')
    require(b'PROX-INSTALL-PREVIEW-NO-OTA-NO-FLASH' in worker,'Wrong client variant')
    for forbidden in (b'flash0:',b'flash1:',b'install_cfw',b'pspDebugScreen',b'sceDisplay',b'sceGu'):
        require(forbidden not in probe,'Unexpected install/render capability in preview module')
    require((output/'PSP/GAME/PROXUPDATE/native-preview-reboot.prx').read_bytes()==(tree/'Rebootex/Rebootex.prx').read_bytes(),'Rebooter not from this build')
    for filename in ('native-install-preview.prx','native-preview-reboot.prx'):
        require(sha((output/'PSP/GAME/PROXUPDATE'/filename).read_bytes()).encode() in worker,'Diagnostic module hash not bound in client')
    completion_header=(ROOT/'firmware/Common/prox_completion_preview.h').read_text()
    resource_hash=re.search(r'PROX_COMPLETION_RCO_SHA256 "([0-9a-f]{64})"',completion_header).group(1)
    resource_size=int(re.search(r'PROX_COMPLETION_RCO_SIZE (\d+)u',completion_header).group(1))
    require(resource_hash.encode() in worker,'Original completion resource is not hash-bound in client')
    name='pro-x-'+VERSION+'-661-native-install-preview'
    files={'PSP/'+p:(output/'PSP'/p).read_bytes() for p in inventory(output/'PSP')}
    files['NATIVE-INSTALL-PREVIEW.md']=(ROOT/'scripts/resources/NATIVE-INSTALL-PREVIEW.md').read_bytes()
    for p in ('LICENSE','THIRD_PARTY_NOTICES.md'): files[p]=(ROOT/p).read_bytes()
    sources={}
    for folder in ('firmware','scripts','tests','docs'):
        for p in inventory(ROOT/folder): sources[folder+'/'+p]=(ROOT/folder/p).read_bytes()
    for p in ('AGENTS.md','README.md','CHANGELOG.md','LICENSE','THIRD_PARTY_NOTICES.md','.gitattributes','.gitignore'): sources[p]=(ROOT/p).read_bytes()
    sources['source-snapshot.json']=(json.dumps({'complete_build_inputs':True,'native_install_preview':True,
        'firmware_tree_sha256':build['source_tree_sha256'],'files':{n:sha(v) for n,v in sources.items()}},indent=2)+'\n').encode()
    for n,v in {**files,**sources}.items():
        require(not n.lower().endswith('.rco') and sha(v)!=resource_hash,'Original Sony resource must remain outside distributable packages')
    dest=ROOT/'builds';dest.mkdir(exist_ok=True)
    sourcepath=dest/(name+'-source.zip'); archive(sourcepath,sources)
    manifest={'product':'PRO-X','version':VERSION,'firmware_target':'6.61','build_id':build['build_id'],
        'source_commit':build['repository_head'],'source_tree_sha256':build['source_tree_sha256'],
        'toolchain_image':build['toolchain_image'],'status':'NO-FLASH DIAGNOSTIC; ORIGINAL-UI BACKGROUND/RESTART NOT YET PSP-VALIDATED',
        'hardware':'NOT RUN','internet_update':False,'background_installation':False,'simulated_offer':True,
        'completion_preview':{'original_text':True,'original_standalone_installer_layout':False,
            'automatic_restart':False,'restart_buttons':['CROSS'],'native_back_navigation':False,'psp_validation':'NOT RUN',
            'external_owner_private_input':{'path':'PSP/GAME/PROXUPDATE/native-completion.rco',
                'bytes':resource_size,'sha256':resource_hash,'included_in_package':False}},
        'source_archive':{'file':sourcepath.name,'sha256':sha(sourcepath.read_bytes())},
        'files':{n:sha(v) for n,v in files.items()},'embedded_modules':checked['embedded_modules'],
        'limitation':'Manual Installer installs this diagnostic CFW normally; the Settings preview performs no download, stage, flash write or version upgrade; production single-consent installation remains pending'}
    files['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    files['SHA256SUMS']=''.join(sha(v)+'  '+n+'\n' for n,v in sorted(files.items())).encode()
    binary=dest/(name+'.zip');archive(binary,files)
    compact=dest/(name+'-build.json'); compact.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    sums=''.join(sha(p.read_bytes())+'  '+p.name+'\n' for p in (binary,sourcepath,compact))
    sums+=''.join(sha(v)+'  '+n+'\n' for n,v in sorted(files.items()) if n.startswith('PSP/'))
    (dest/(name+'.sha256')).write_text(sums,encoding='ascii')
    for p,v in files.items():
        if p.startswith('PSP/'):
            target=dest/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(v)
    with zipfile.ZipFile(binary) as zipped:
        require(len(zipped.namelist())==len(files),'Duplicate package entry')
        for n,v in files.items(): require(zipped.read(n)==v,'Reopened package mismatch')
    print('PASS: exact compiled diagnostic, no-flash/no-render module, bound module hashes, same-build rebooter, reopened inventory')
    print(sums,end='')
if __name__=='__main__':main()
