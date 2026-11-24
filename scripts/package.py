"""Stage checked firmware, OTA assets and their companion source archive."""
from pathlib import Path
import json
import zipfile
import subprocess
import sys
import tarfile
from build import ROOT, inventory, sha, repository_head
from verify import require
from product import VERSION, WPA2_VARIANT, WPA2_REPORT_SHA256
from hardware_report import REPORT as HARDWARE_REPORT
from update_contract import generate as generate_update
from native_release import verify_native_release
from native_validation_report import for_build as native_validation

NAME = 'pro-x-' + VERSION + '-661'
OUTPUT_DIR = ROOT / 'builds'
BUILTIN_COMPLETION = {'source':'resident updater','languages':12,'external_resource_required':False}
PRIVATE_COMPLETION_HASHES = {
    '714c0af3c51f61a895311ee1e58e719d8ed7fc5d3e5e441de38d67133617c265',
    '4b47ddf778c8df121a7fe3ade8acaada307ffd3e96d17834f568e29240557ba7',
}


def archive(path, files):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as stream:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, date_time=(2026, 11, 13, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            stream.writestr(entry, data)


def dedicated_assets(output, destination):
    """Copy the exact dedicated contract verified against the compiled parts."""
    compiled=output/'dedicated-update'
    assets=[]
    for name in ('EBOOT.PBP','pro-x-dedicated-update.json'):
        asset=destination/name
        asset.write_bytes((compiled/name).read_bytes())
        assets.append(asset)
    return assets


def main():
    subprocess.run([sys.executable, str(ROOT / 'scripts/verify.py')], check=True, cwd=ROOT)
    build = json.loads((ROOT / 'artifacts/pro-x-build.json').read_text())
    verify_native_release(ROOT,build)
    native_report=native_validation(build)
    native_status=native_report['status'] if native_report else 'NOT RUN'
    require(build.get('product_version') == VERSION, 'Product version differs from compiled build')
    resident=build.get('resident_dedicated_ota',False)
    destination=OUTPUT_DIR/('resident-release-'+build['build_id']) if resident else OUTPUT_DIR
    destination.mkdir(parents=True,exist_ok=True)
    source_commit = repository_head()
    reference = json.loads((ROOT / 'artifacts/reference-build.json').read_text())
    verification = json.loads((ROOT / 'artifacts/verification.json').read_text())
    sources = {}
    for name in inventory(ROOT / 'firmware'):
        sources['firmware/' + name] = (ROOT / 'firmware' / name).read_bytes()
    for directory in ['scripts', 'tests', 'docs']:
        for name in inventory(ROOT / directory):
            sources[directory + '/' + name] = (ROOT / directory / name).read_bytes()
    for name in ['README.md', 'AGENTS.md', 'LICENSE', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md', '.gitattributes', '.gitignore']:
        sources[name] = (ROOT / name).read_bytes()
    background=bool(build.get('native_background'))
    completion = {
        'path':'PSP/GAME/PROXUPDATE/native-completion.rco','bytes':377448,
        'sha256':'714c0af3c51f61a895311ee1e58e719d8ed7fc5d3e5e441de38d67133617c265',
        'included_in_package':False,
    } if background else None
    if resident:
        completion = BUILTIN_COMPLETION
    completion_note = ('The resident updater supplies its own localized completion messages. Setup and Internet updates require no separate resource file.\n\n'
                       if resident else
                       'Native OTA uses a matched background writer; see the exact-build owner-reported validation scope in manifest.json. The separate owner-supplied original completion resource is required at runtime and is not included.\n\n'
                       if background else 'The existing target installer retains its text screen and X confirmations.\n\n')
    build_mode=' --resident-dedicated-ota' if resident else ''
    source_note = (
        f'PRO-X v{VERSION} / 6.61: original native System Update frontend and verified GitHub updater.\n'
        'See the exact-build owner-report scope in manifest.json; do not infer universal model/router or Infinity validation.\n'
        + completion_note +
        'This archive contains the exact inputs needed to build Installer and Fast Recovery, including inherited libraries/March33, scripts, notices and documentation.\n'
        'Inherited source tools are preserved; compilation and the binary package omit IPL/permanent/dumping programs. No Sony firmware or Infinity is included.\n'
        'See docs/upstream.md for upstream SHA, authorship and inherited binary limitations.\n'
        'Build: docker build -t pro-x-toolchain:phase1 -f scripts/Dockerfile scripts\n'
        f'       python scripts/build.py pro-x{build_mode}\n'
        'Compare the resulting EBOOT files with the hashes in the binary package manifest.\n'
        'Full source-scope verification uses the complete repository checkout and reference build.\n'
    ).encode()
    sources['SOURCE.md'] = source_note
    source_hashes = {name: sha(data) for name, data in sorted(sources.items())}
    sources['source-snapshot.json'] = (json.dumps({
        'upstream_revision': build['upstream_revision'], 'firmware_tree_sha256': build['source_tree_sha256'],
        'repository_head': source_commit, 'build_repository_head': build['repository_head'],
        'files': source_hashes, 'complete_build_inputs': True, 'complete_upstream_tree': True,
    }, indent=2) + '\n').encode()
    source_zip = destination / (NAME + '-source.zip')
    archive(source_zip, sources)

    files = {}
    card_root = 'release/' if resident else ''
    psp_prefix = card_root + 'PSP/'
    menu_prefix = card_root + ('SEPLUGINS/' if resident else 'seplugins/')
    output = ROOT / build['output']
    executables=('PROUPDATE','FastRecovery') if resident else ('PROUPDATE','FastRecovery','PROXUPDATE')
    for directory in executables:
        source_name = 'PSP/GAME/' + directory + '/EBOOT.PBP'
        name = card_root + source_name
        files[name] = (output / source_name).read_bytes()
        executable = destination / name
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_bytes(files[name])
    if not resident:
        name = 'PSP/GAME/PROXUPDATE/cacert.pem'
        files[name] = (output / name).read_bytes()
        (destination / name).write_bytes(files[name])
    guide='RESIDENT-DEDICATED-OTA.md' if resident else 'NATIVE-UPDATE.md'
    files[guide] = ((ROOT/'scripts/resources'/guide).read_bytes()
                    .replace(b'@PRODUCT_VERSION@', VERSION.encode('ascii'))
                    .replace(b'@CARD_ROOT@', (card_root + 'PSP').encode('ascii')))
    with tarfile.open(ROOT / 'firmware/Updater/vendor/mbedtls-3.6.7.tar.bz2') as tls_source:
        for entry in tls_source:
            parts = Path(entry.name).parts[1:]
            if entry.isfile() and parts and ('LICENSE' in parts[0] or 'COPYING' in parts[0]):
                files['licenses/mbedtls/'+'/'.join(parts)] = tls_source.extractfile(entry).read()
    for name in inventory(ROOT / 'firmware/Translated'):
        files[menu_prefix + name] = (ROOT / 'firmware/Translated' / name).read_bytes()
    for name in inventory(ROOT / 'firmware/contrib/fonts'):
        files[menu_prefix + 'fonts/' + name] = (ROOT / 'firmware/contrib/fonts' / name).read_bytes()
    if resident:
        for name, data in files.items():
            if name.startswith(menu_prefix):
                resource = destination / name
                resource.parent.mkdir(parents=True, exist_ok=True)
                resource.write_bytes(data)
    for name in ['LICENSE', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md']:
        files[name] = (ROOT / name).read_bytes()
    for name in ['build.md', 'upstream.md', 'install-and-validation.md', 'phase-1-results.md',
                 'wpa2-support.md', 'wpa2-validation.md', 'wpa2-upstream.json', 'phase-2-source.json']:
        files['docs/' + name] = (ROOT / 'docs' / name).read_bytes()
    for name in inventory(ROOT / 'docs/licenses'):
        files['licenses/' + name] = (ROOT / 'docs/licenses' / name).read_bytes()
    files['credit.txt'] = (ROOT / 'firmware/credit.txt').read_bytes()
    files['SOURCE.md'] = source_note + ('\nCompanion: ' + source_zip.name + '\nSHA-256: ' + sha(source_zip.read_bytes()) + '\n').encode()
    files['toolchain.txt'] = (ROOT / 'docs/build-toolchain.txt').read_bytes()
    update_assets=dedicated_assets(output,destination) if resident else generate_update(destination,VERSION,build['build_id'],files)
    install_path='dedicated resident EBOOT' if resident else 'background installation' if background else 'existing-installer handoff'
    manifest = {
        'product': 'PRO-X', 'version': VERSION, 'firmware_target': '6.61',
        'status': 'native-update-release-candidate', 'hardware': native_status, 'infinity': 'NOT RUN',
        'native_update_hardware_report': native_report,
        'baseline_hardware_report': HARDWARE_REPORT,
        'phase3': {'milestone': 'native GitHub checking, verified download and '+install_path,
                   'native_frontend': True, 'native_ota': True,
                   'native_background':background,'completion_resource':completion,
                   'online_installation_implemented': True, 'physical_validation': native_status,
                   'transport_baseline': {'build_id': 'pro-x-c41ee2ff7b5e', 'status': 'USER_SUPPLIED_REPORTS_PASS',
                                          'reports': ['report-006', 'report-007', 'report-008']},
                   'update_assets': {p.name: sha(p.read_bytes()) for p in update_assets},
                   'dependencies': json.loads((ROOT / 'scripts/updater-dependencies.json').read_text())},
        'build_id': build['build_id'],
        'wpa2': {'preference_enabled_by_default': True, 'verified_variants': 1,
                 'variant_id': WPA2_VARIANT, 'identification_report_sha256': WPA2_REPORT_SHA256,
                 'patch_active': True, 'status': 'INHERITED_PARTIAL_REPORT_NEW_BUILD_NOT_RUN'},
        'upstream_url': 'https://github.com/MrColdbird/procfw',
        'upstream_revision': build['upstream_revision'], 'repository_head': source_commit,
        'build_repository_head': build['repository_head'],
        'source_tree_sha256': build['source_tree_sha256'], 'source_files': build['source_files'],
        'compile_script_sha256': build['compile_script_sha256'],
        'host_signer_source_files': {n:h for n,h in build['source_files'].items() if n.startswith('contrib/PrxEncrypter/')},
        'dockerfile_sha256': build['dockerfile_sha256'], 'toolchain_image': build['toolchain_image'],
        'toolchain_base': 'ghcr.io/pspdev/pspdev-docker@sha256:c4888f62d5b1d2ff905b14bab664c346af4d72e71e4bb13e815dafa3219eb18a',
        'reference_source_sha256': reference['source_tree_sha256'],
        'source_archive': {'file': source_zip.name, 'sha256': sha(source_zip.read_bytes())},
        'embedded_modules': verification['pro-x']['embedded_modules'],
        'prebuilt_inputs': {name: value for name, value in build['source_files'].items() if name.endswith(('.prx', '.a')) and
                           (name.startswith('libs/') or name.startswith('ISODrivers/March33/'))},
        'pbp_inspection': verification['pro-x']['pbp'],
        'files': {name: sha(data) for name, data in sorted(files.items())},
        'hash_convention': 'files excludes manifest.json and SHA256SUMS; SHA256SUMS covers manifest and all payload files except itself; ZIP hashes are external',
    }
    if completion:
        for name,data in {**files,**sources}.items():
            require(not name.lower().endswith('.rco') and sha(data) not in PRIVATE_COMPLETION_HASHES,
                    'Original completion resource must remain outside distributable packages')
    if resident:
        manifest['memory_stick_directory'] = 'release'
        manifest['ota_contract']='prox-dedicated-eboot-v1'
        manifest['dedicated_update']=json.loads((output/'dedicated-update/dedicated-update.json').read_bytes())
    files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    files['SHA256SUMS'] = ''.join(sha(data) + '  ' + name + '\n' for name, data in sorted(files.items())).encode()
    binary_zip = destination / (NAME + '.zip')
    archive(binary_zip, files)
    manifest_path = destination / (NAME + '-manifest.json')
    manifest_path.write_bytes(files['manifest.json'])
    compact_path = destination / (NAME + '-build.json')
    compact = {
        'product': 'PRO-X', 'version': VERSION, 'firmware_target': '6.61',
        'build_id': build['build_id'], 'status': 'native-update-release-candidate', 'hardware': native_status,
        'native_update_hardware_report': native_report,
        'repository_head': build['repository_head'], 'source_tree_sha256': build['source_tree_sha256'],
        'source_identity': 'working tree identified by hashes; HEAD alone is not this build',
        'toolchain_image': build['toolchain_image'],
        'compile_script_sha256': build['compile_script_sha256'],
        'host_signer_source_files': manifest['host_signer_source_files'],
        'updater_compile_script_sha256': sha((ROOT / 'scripts/compile-updater.sh').read_bytes()),
        'packages': {p.name: sha(p.read_bytes()) for p in (binary_zip,source_zip,*update_assets)},
        'detailed_manifest_sha256': sha(files['manifest.json']),
        'executables': {n: {'size': len(d), 'sha256': sha(d)} for n, d in files.items() if n.endswith('/EBOOT.PBP')},
    }
    compact_path.write_text(json.dumps(compact, indent=2)+'\n', encoding='utf-8')
    external = ''.join(sha(path.read_bytes()) + '  ' + path.name + '\n'
                       for path in (binary_zip,source_zip,*update_assets,manifest_path,compact_path))
    external += ''.join(sha(data)+'  '+name+'\n' for name, data in sorted(files.items()) if name.startswith(psp_prefix))
    (destination / (NAME + '.sha256')).write_text(external, encoding='ascii')
    if resident:
        print(external,end='')
        return
    # This opt-in tool is separate from the normal two-program installation ZIP.
    diagnostics = {name: (output / 'diagnostics' / name).read_bytes() for name in build['diagnostics']}
    diagnostics['docs/wpa2-support.md'] = (ROOT / 'docs/wpa2-support.md').read_bytes()
    diagnostics['LICENSE'] = (ROOT / 'LICENSE').read_bytes()
    diagnostics['THIRD_PARTY_NOTICES.md'] = (ROOT / 'THIRD_PARTY_NOTICES.md').read_bytes()
    diagnostics['source-reference.json'] = (json.dumps(manifest['source_archive'],indent=2)+'\n').encode()
    diagnostics['SHA256SUMS'] = ''.join(sha(data)+'  '+name+'\n' for name,data in sorted(diagnostics.items())).encode()
    diagnostic_zip = destination / (NAME + '-diagnostic.zip')
    archive(diagnostic_zip,diagnostics)
    diagnostic_zip.with_suffix('.sha256').write_text(sha(diagnostic_zip.read_bytes())+'  '+diagnostic_zip.name+'\n',encoding='ascii')
    print(external, end='')


if __name__ == '__main__':
    main()
