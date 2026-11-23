"""Package the compiled real native GitHub query milestone, without OTA assets."""
import hashlib
import argparse
import json
from pathlib import Path
import tarfile
import zipfile
from build import ROOT, inventory, sha
from package import archive
from product import VERSION
from verify import require, verify_source, verify_build


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offer-preview',action='store_true')
    parser.add_argument('--progress-preview',action='store_true')
    parser.add_argument('--native-ota',action='store_true')
    args=parser.parse_args()
    if args.native_ota and (args.offer_preview or args.progress_preview): parser.error('select one mode')
    if args.progress_preview:
        if args.offer_preview: parser.error('select one diagnostic mode')
        args.offer_preview=True
    build = json.loads((ROOT/'artifacts/pro-x-build.json').read_text())
    require(build.get('native_query_only') is True or build.get('native_ota') is True, 'Not a native test build')
    preview=build.get('native_offer_preview',False)
    progress=build.get('native_progress_preview',False)
    ota=build.get('native_ota',False)
    background=build.get('native_background',False)
    require(not build.get('native_install_preview'),'Use the explicit no-flash packager')
    require(ota==args.native_ota,'Select the matching native OTA package mode')
    require(preview==args.offer_preview,'Select the matching diagnostic package mode')
    require(progress==args.progress_preview,'Select the matching progress package mode')
    require(build['product_version'] == VERSION, 'Product version mismatch')
    require(inventory(ROOT/'firmware') == build['source_files'], 'Firmware changed after compilation')
    reference = json.loads((ROOT/'artifacts/reference-build.json').read_text())
    require(reference['toolchain_image'] == build['toolchain_image'], 'Toolchain mismatch')
    verify_source(reference)
    checked = verify_build(build)
    output = ROOT/build['output']
    vsh = (output/'build-tree/Vshctrl/vshctrl.elf').read_bytes()
    client = (output/'build-tree/Updater/prox_updater.elf').read_bytes()
    marker=b'native REAL compatible offer; awaiting consent' if ota else b'PREVIEW simulated offer; native writes blocked' if preview else b'native query REAL current'
    require(marker in vsh and b'native probe simulated' not in vsh, 'Wrong native provider')
    require(b'PROX-NATIVE-QUERY-V3' in client, 'Missing real-service marker')
    if ota:
        require(b'PROX-NATIVE-CARDLESS-V1' in client, 'Missing resident trust/bootstrap contract')
        if background:
            require(b'PROX-NATIVE-DATA-ONLY-V1' in client and b'kuKernelLoadModuleWithApitype2' not in client,
                    'Missing data-only target decoder or obsolete target loader linked')
        require(client.count((ROOT/'firmware/Updater/cacert.pem').read_bytes()+b'\0')==1,
                'Resident CA differs from the pinned manual-package bundle')
        require(b'native query RAM prepared' in vsh and b'native query RAM module load' in vsh,
                'Missing card-independent bootstrap')
        require(b'native query RAM: size-first load' in vsh and b'sceKernelLoadModuleBuffer\0' not in vsh,
                'Unsafe legacy buffer-loader import or missing native 6.61 ABI adapter')
    require(b'ProxNativeQueryForUser' in vsh and b'ProxNativeQueryForUser' in client, 'IPC library missing')
    for symbol in ('InitEx', 'Setting', 'Connect', 'Read', 'Abort', 'End', 'Service'):
        require(hashlib.sha1(('proxNativeQuery'+symbol).encode()).digest()[:4] in vsh, 'Missing private syscall')
    if preview:
        require(b'PREVIEW simulated offer; native writes blocked' in vsh and b'ProxNativePreviewForUser' in vsh,
                'Missing preview identity or I/O guards')
        for symbol in ('Deny','Open','Ioctl','Devctl'):
            require(hashlib.sha1(('proxNativePreview'+symbol).encode()).digest()[:4] in vsh,'Missing preview guard syscall')
    if progress:
        require(b'PREVIEW progress completion blocked' in vsh and b'PREVIEW virtual update opened; no disk file' in vsh,
                'Missing incomplete virtual stream identity')
        for symbol in ('Write','Close','Noop'):
            require(hashlib.sha1(('proxNativePreview'+symbol).encode()).digest()[:4] in vsh,'Missing virtual writer syscall')
    if ota:
        require(b'msgshare_error_notplay_ms' in vsh and b'msgupdate_error_start' in vsh,
                'Missing existing native error resources')
        require(hashlib.sha1(b'proxNativeQueryTextKey').digest()[:4] in vsh,'Missing native error key syscall')
        require(b'PREVIEW simulated offer' not in vsh and b'native consent starts REAL private download' in vsh,
                'Native OTA cannot simulate an eligible release')
        require((b'PROX-NATIVE-BACKGROUND-INSTALL-V1' if background else b'Verified native handoff to target installer') in client and b'ProxNativePreviewForUser' in vsh,
                'Missing real handoff or virtual filesystem guards')
        for symbol in ('Deny','Open','Ioctl','Devctl','Write','Close','Noop'):
            require(hashlib.sha1(('proxNativePreview'+symbol).encode()).digest()[:4] in vsh,'Missing confined native writer syscall')
    sources = {}
    for directory in ('firmware', 'scripts', 'tests', 'docs'):
        for name in inventory(ROOT/directory):
            sources[directory+'/'+name] = (ROOT/directory/name).read_bytes()
    for name in ('README.md', 'CHANGELOG.md', 'AGENTS.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', '.gitignore', '.gitattributes'):
        sources[name] = (ROOT/name).read_bytes()
    kind='real-native-background-install-test' if background else 'real-native-ota-legacy-installer-test' if ota else 'simulated-native-progress-preview' if progress else 'simulated-native-offer-preview' if preview else 'real-native-github-query-only'
    manifest = dict(build, kind=kind, production_ota=False,
                    native_installation=ota, simulated_positive_offer=preview, simulated_incomplete_transfer=progress,
                    legacy_target_installer=ota and not background, final_native_installer_ui=False,
                    native_background=background, hardware='NOT RUN')
    if background:
        manifest['completion_resource']={'path':'PSP/GAME/PROXUPDATE/native-completion.rco','bytes':377448,
            'sha256':'714c0af3c51f61a895311ee1e58e719d8ed7fc5d3e5e441de38d67133617c265','included_in_package':False}
    sources['source-snapshot.json'] = (json.dumps(manifest, indent=2)+'\n').encode()
    sources['SOURCE.md'] = ('Exact firmware/build inputs for '+build['build_id']+'.\n'
                           'Build: python scripts/build.py pro-x '+('--native-ota' if ota else '--native-progress-preview' if progress else '--native-offer-preview' if preview else '--native-query')+'\n'
                           'Package: python scripts/package-native-query.py'+(' --native-ota' if ota else ' --progress-preview' if progress else ' --offer-preview' if preview else '')+'\n'
                           'This archive contains no Sony firmware/resource dump or Infinity.\n').encode()
    destination = ROOT/'builds'
    destination.mkdir(exist_ok=True)
    name = 'pro-x-'+VERSION+'-661-'+('native-ota-test' if ota else 'native-progress-preview' if progress else 'native-offer-preview' if preview else 'native-github-query')
    source_zip = destination/(name+'-source.zip')
    archive(source_zip, sources)
    files = {'PSP/'+n: (output/'PSP'/n).read_bytes() for n in inventory(output/'PSP')}
    for item in ('LICENSE', 'THIRD_PARTY_NOTICES.md', 'CHANGELOG.md'):
        files[item] = (ROOT/item).read_bytes()
    for item in inventory(ROOT/'docs/licenses'):
        files['licenses/'+item] = (ROOT/'docs/licenses'/item).read_bytes()
    with tarfile.open(ROOT/'firmware/Updater/vendor/mbedtls-3.6.7.tar.bz2') as tls:
        for entry in tls:
            relative = Path(*Path(entry.name).parts[1:])
            if entry.isfile() and relative.parts and relative.parts[0].startswith(('LICENSE', 'COPYING')):
                files['licenses/mbedtls/'+relative.as_posix()] = tls.extractfile(entry).read()
    instructions='NATIVE-BACKGROUND-TEST.md' if background else 'NATIVE-OTA-TEST.md' if ota else 'NATIVE-PROGRESS-PREVIEW.md' if progress else 'NATIVE-OFFER-PREVIEW.md' if preview else 'NATIVE-GITHUB-QUERY.md'
    files[instructions] = (ROOT/'scripts/resources'/instructions).read_bytes().replace(b'@PRODUCT_VERSION@',VERSION.encode('ascii'))
    files['SOURCE.md'] = (source_zip.name+'\nSHA-256: '+sha(source_zip.read_bytes())+'\n').encode()
    files['build-manifest.json'] = (json.dumps(manifest, indent=2)+'\n').encode()
    if background:
        for n,data in {**files,**sources}.items():
            require(not n.lower().endswith('.rco') and sha(data)!=manifest['completion_resource']['sha256'],
                    'Original resource must remain outside test/source packages')
    binary_zip = destination/(name+'.zip')
    archive(binary_zip, files)
    for path, expected in ((binary_zip, files), (source_zip, sources)):
        with zipfile.ZipFile(path) as stream:
            require(set(stream.namelist()) == set(expected), 'Unexpected package inventory')
            require(all(stream.read(n) == data for n, data in expected.items()), 'Package bytes mismatch')
    for relative, data in files.items():
        path = destination/relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    compact=destination/(name+'-build.json');compact.write_bytes(files['build-manifest.json'])
    hashes = ''.join(sha(p.read_bytes())+'  '+p.name+'\n' for p in (binary_zip, source_zip,compact))
    hashes += ''.join(sha(files[n])+'  '+n+'\n' for n in sorted(files) if n.startswith('PSP/'))
    (destination/(name+'.sha256')).write_text(hashes, encoding='ascii')
    proof = dict(status='PASS', build_id=build['build_id'], source_scope='PASS', compiled=checked,
                 simulated_positive_offer=preview,
                 simulated_incomplete_transfer=progress,
                 packages='PASS', source_replay='NOT RUN', hardware='NOT RUN', native_installation='NOT RUN' if ota else 'BLOCKED')
    proof_path=ROOT/'artifacts/phase3/native-query-verification.json'
    proof_path.parent.mkdir(parents=True,exist_ok=True)
    proof_path.write_text(json.dumps(proof, indent=2)+'\n')
    print('PASS: compiled provenance, labels, scope, private syscalls and reopened native test packages')
    print(hashes)


if __name__ == '__main__':
    main()
