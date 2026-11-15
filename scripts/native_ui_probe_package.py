"""Package only a compiled native-UI firmware diagnostic; never OTA assets."""
from pathlib import Path
import hashlib,json,shutil,zipfile
from build import ROOT,inventory,digest,sha
from package import archive
from verify import require,verify_source,verify_build
from product import VERSION

def package_probe(build):
    require(build['kind']=='original-native-ui-diagnostic' and not build['production_ota'], 'Wrong build kind')
    require(build['product_version']==VERSION, 'Product version mismatch')
    require(inventory(ROOT/'firmware')==build['source_files'],'Firmware changed since diagnostic compilation')
    verify_source(json.loads((ROOT/'artifacts/reference-build.json').read_text()))
    checked=verify_build(build); output=ROOT/build['output']
    elf=(output/'build-tree/Vshctrl/vshctrl.elf').read_bytes()
    require(b'ProxNativeUiProbeForUser' in elf,'Missing syscall registration library')
    for symbol in ('InitEx','Setting','Connect','Read','Abort','End'):
        require(hashlib.sha1(('proxNativeProbe'+symbol).encode()).digest()[:4] in elf,'Missing syscall NID')
    sources={}
    for directory in ('firmware','scripts','tests','docs'):
        for name in inventory(ROOT/directory): sources[directory+'/'+name]=(ROOT/directory/name).read_bytes()
    for name in ('README.md','CHANGELOG.md','LICENSE','THIRD_PARTY_NOTICES.md','AGENTS.md','.gitignore'):
        sources[name]=(ROOT/name).read_bytes()
    sources['source-snapshot.json']=(json.dumps(build,indent=2)+'\n').encode()
    name=f'pro-x-{VERSION}-661-native-ui-probe-pr'
    (ROOT/'builds').mkdir(parents=True,exist_ok=True)
    source_zip=ROOT/'builds'/(name+'-source.zip'); archive(source_zip,sources)
    files={}
    for directory in ('PROUPDATE','FastRecovery'):
        relative='PSP/GAME/'+directory+'/EBOOT.PBP'; files[relative]=(output/relative).read_bytes()
    for license_name in ('LICENSE','THIRD_PARTY_NOTICES.md'): files[license_name]=(ROOT/license_name).read_bytes()
    for license_name in inventory(ROOT/'docs/licenses'):
        files['licenses/'+license_name]=(ROOT/'docs/licenses'/license_name).read_bytes()
    files['NATIVE-UPDATE-PROBE.md']=(ROOT/'scripts/resources/NATIVE-UPDATE-PROBE.md').read_bytes()
    files['build-manifest.json']=(json.dumps(build,indent=2)+'\n').encode()
    files['SOURCE.md']=(f'Companion: {source_zip.name}\nSHA-256: {sha(source_zip.read_bytes())}\n'
                       'Compile with PROX_NATIVE_UPDATE_FLAGS=-DPROX_NATIVE_UI_PROBE=1.\n').encode()
    destination=ROOT/'builds/native-ui-probe-pr'
    for relative,data in files.items():
        path=destination/relative; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
    binary_zip=ROOT/'builds'/(name+'.zip'); archive(binary_zip,files)
    for path,expected in ((binary_zip,files),(source_zip,sources)):
        with zipfile.ZipFile(path) as z:
            require(set(z.namelist())==set(expected),'Unexpected package contents')
            require(all(z.read(n)==d for n,d in expected.items()),'Package bytes differ')
    # Preserve the previous canonical generated files before replacing them.
    canonical=ROOT/'builds/PSP'
    if canonical.exists():
        previous=inventory(canonical)
        history=ROOT/'builds/history'/('canonical-'+digest(previous)[:12])/'PSP'
        shutil.copytree(canonical,history,dirs_exist_ok=True)
        require(inventory(history)==previous,'Canonical history backup differs')
    shutil.copytree(output/'PSP',canonical,dirs_exist_ok=True)
    actual=inventory(canonical)
    firmware_outputs={n:v for n,v in actual.items() if Path(n).parts[1] in ('PROUPDATE','FastRecovery','PROXUPDATE')}
    require(firmware_outputs==inventory(output/'PSP'),'Mixed canonical firmware outputs')
    (ROOT/'builds/CURRENT-BUILD-IS-NATIVE-UI-PROBE.txt').write_text(
        'Current canonical PSP files belong to diagnostic '+build['build_id']+'.\n'
        'Use only the two executables in the native-ui-probe ZIP and follow its instructions.\n'
        'The Internet-query result is simulated; this is not a production OTA build.\n')
    hashes=''.join(sha(p.read_bytes())+'  '+p.name+'\n' for p in (binary_zip,source_zip))
    hashes+=''.join(sha(files[n])+'  native-ui-probe-pr/'+n+'\n' for n in files if n.endswith('EBOOT.PBP'))
    (ROOT/'builds'/(name+'.sha256')).write_text(hashes,encoding='ascii')
    proof={'status':'PASS','build_id':build['build_id'],'kind':build['kind'],'compiled':checked,
           'source_replay':'NOT RUN','hardware':'NOT RUN','simulated_catalog_result':True}
    (ROOT/'artifacts/phase3/native-ui-probe-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
    print('PASS: diagnostic packages reopened; matching canonical files and history verified')
    print(hashes)

if __name__=='__main__':
    package_probe(json.loads((ROOT/'artifacts/native-ui-probe-build.json').read_text()))
