"""Create an explicit experimental payload and companion build-source archive."""
from pathlib import Path
import json
import zipfile
import subprocess
import sys
import tarfile
from build import ROOT, inventory, digest, sha, repository_head
from verify import require
from product import VERSION, WPA2_VARIANT, WPA2_REPORT_SHA256
from hardware_report import REPORT as HARDWARE_REPORT
from update_contract import generate as generate_update

NAME = 'pro-x-' + VERSION + '-661'
OUTPUT_DIR = ROOT / 'builds'
SOURCE_DIRS = {'Common', 'CrossFW', 'FastRecovery', 'Imports', 'Installer',
               'ISODrivers', 'Popcorn', 'PXE', 'Rebootex', 'Rebootex_bin',
               'Recovery', 'Satelite', 'Stargate', 'SystemControl', 'Translated',
               'Vshctrl', 'include', 'libs', 'usbdevice', 'tools', 'Updater'}


def archive(path, files):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as stream:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, date_time=(2026, 11, 13, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            stream.writestr(entry, data)


def main():
    subprocess.run([sys.executable, str(ROOT / 'scripts/verify.py')], check=True, cwd=ROOT)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    build = json.loads((ROOT / 'artifacts/pro-x-build.json').read_text())
    require(build.get('product_version') == VERSION, 'Product version differs from compiled build')
    source_commit = repository_head()
    reference = json.loads((ROOT / 'artifacts/reference-build.json').read_text())
    verification = json.loads((ROOT / 'artifacts/verification.json').read_text())
    sources = {}
    for name in inventory(ROOT / 'firmware'):
        parts = Path(name).parts
        if (parts[0] in SOURCE_DIRS or name in ['Permanent/ppatch_config.h', 'Makefile', 'README.txt', 'credit.txt']
                or name == 'contrib/pspgz.py' or name == 'contrib/SystemControl.hdr' or name.startswith('contrib/fonts/')):
            sources['firmware/' + name] = (ROOT / 'firmware' / name).read_bytes()
    for directory in ['scripts', 'tests', 'docs']:
        for name in inventory(ROOT / directory):
            sources[directory + '/' + name] = (ROOT / directory / name).read_bytes()
    for name in ['README.md', 'LICENSE', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md', '.gitattributes', '.gitignore']:
        sources[name] = (ROOT / name).read_bytes()
    source_note = (
        f'PRO-X v{VERSION} / 6.61: GitHub updater test candidate; hardware NOT RUN for this build.\n'
        'The earlier transport prototype passed owner-supplied HTTPS/asset tests.\n'
        'The new release/staging/installer flow still needs physical validation.\n'
        'WPA2 inherits the previous limited report; this build has not been hardware tested.\n\n'
        'This archive contains the exact inputs needed to build Installer and Fast Recovery,\n'
        'including inherited libraries/March33, scripts, notices and documentation.\n'
        'Unrelated IPL tools and dumping utilities are omitted. No Sony firmware or Infinity is included.\n'
        'See docs/upstream.md for upstream SHA, authorship and inherited binary limitations.\n'
        'Build: docker build -t pro-x-toolchain:phase1 -f scripts/Dockerfile scripts\n'
        '       python scripts/build.py pro-x\n'
        'Compare the resulting EBOOT files with the hashes in the binary package manifest.\n'
        'Full source-scope verification uses the complete repository checkout and reference build.\n'
    ).encode()
    sources['SOURCE.md'] = source_note
    source_hashes = {name: sha(data) for name, data in sorted(sources.items())}
    sources['source-snapshot.json'] = (json.dumps({
        'upstream_revision': build['upstream_revision'], 'firmware_tree_sha256': build['source_tree_sha256'],
        'repository_head': source_commit, 'build_repository_head': build['repository_head'],
        'files': source_hashes, 'complete_build_inputs': True, 'complete_upstream_tree': False,
    }, indent=2) + '\n').encode()
    source_zip = OUTPUT_DIR / (NAME + '-source.zip')
    archive(source_zip, sources)

    files = {}
    output = ROOT / build['output']
    for directory in ['PROUPDATE', 'FastRecovery', 'PROXUPDATE']:
        name = 'PSP/GAME/' + directory + '/EBOOT.PBP'
        files[name] = (output / name).read_bytes()
        executable = OUTPUT_DIR / name
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_bytes(files[name])
    name = 'PSP/GAME/PROXUPDATE/cacert.pem'
    files[name] = (output / name).read_bytes()
    (OUTPUT_DIR / name).write_bytes(files[name])
    files['UPDATE-TEST.md'] = (ROOT / 'scripts/resources/UPDATE-TEST.md').read_bytes()
    with tarfile.open(ROOT / 'firmware/Updater/vendor/mbedtls-3.6.7.tar.bz2') as tls_source:
        for entry in tls_source:
            parts = Path(entry.name).parts[1:]
            if entry.isfile() and parts and ('LICENSE' in parts[0] or 'COPYING' in parts[0]):
                files['licenses/mbedtls/'+'/'.join(parts)] = tls_source.extractfile(entry).read()
    for name in inventory(ROOT / 'firmware/Translated'):
        files['seplugins/' + name] = (ROOT / 'firmware/Translated' / name).read_bytes()
    for name in inventory(ROOT / 'firmware/contrib/fonts'):
        files['seplugins/fonts/' + name] = (ROOT / 'firmware/contrib/fonts' / name).read_bytes()
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
    update_zip, update_json = generate_update(OUTPUT_DIR, VERSION, build['build_id'], files)
    manifest = {
        'product': 'PRO-X', 'version': VERSION, 'firmware_target': '6.61',
        'status': 'phase3-update-test-candidate', 'hardware': 'NOT RUN', 'infinity': 'NOT RUN',
        'baseline_hardware_report': HARDWARE_REPORT,
        'phase3': {'milestone': 'stable selection, verified staging and installer candidate',
                   'online_installation_implemented': True, 'physical_validation': 'NOT RUN for this candidate',
                   'transport_baseline': {'build_id': 'pro-x-c41ee2ff7b5e', 'status': 'USER_SUPPLIED_REPORTS_PASS',
                                          'reports': ['report-006', 'report-007', 'report-008']},
                   'update_assets': {p.name: sha(p.read_bytes()) for p in (update_zip,update_json)},
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
    files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    files['SHA256SUMS'] = ''.join(sha(data) + '  ' + name + '\n' for name, data in sorted(files.items())).encode()
    binary_zip = OUTPUT_DIR / (NAME + '.zip')
    archive(binary_zip, files)
    manifest_path = OUTPUT_DIR / (NAME + '-manifest.json')
    manifest_path.write_bytes(files['manifest.json'])
    compact_path = OUTPUT_DIR / (NAME + '-build.json')
    compact = {
        'product': 'PRO-X', 'version': VERSION, 'firmware_target': '6.61',
        'build_id': build['build_id'], 'status': 'phase3-update-test-candidate', 'hardware': 'NOT RUN',
        'repository_head': build['repository_head'], 'source_tree_sha256': build['source_tree_sha256'],
        'source_identity': 'working tree identified by hashes; HEAD alone is not this build',
        'toolchain_image': build['toolchain_image'],
        'compile_script_sha256': build['compile_script_sha256'],
        'updater_compile_script_sha256': sha((ROOT / 'scripts/compile-updater.sh').read_bytes()),
        'packages': {p.name: sha(p.read_bytes()) for p in [binary_zip, source_zip, update_zip, update_json]},
        'detailed_manifest_sha256': sha(files['manifest.json']),
        'executables': {n: {'size': len(d), 'sha256': sha(d)} for n, d in files.items() if n.endswith('/EBOOT.PBP')},
    }
    compact_path.write_text(json.dumps(compact, indent=2)+'\n', encoding='utf-8')
    external = ''.join(sha(path.read_bytes()) + '  ' + path.name + '\n'
                       for path in [binary_zip, source_zip, update_zip, update_json, manifest_path, compact_path])
    external += ''.join(sha(data)+'  '+name+'\n' for name, data in sorted(files.items()) if name.startswith('PSP/'))
    (OUTPUT_DIR / (NAME + '.sha256')).write_text(external, encoding='ascii')
    # This opt-in tool is separate from the normal two-program installation ZIP.
    diagnostics = {name: (output / 'diagnostics' / name).read_bytes() for name in build['diagnostics']}
    diagnostics['docs/wpa2-support.md'] = (ROOT / 'docs/wpa2-support.md').read_bytes()
    diagnostics['LICENSE'] = (ROOT / 'LICENSE').read_bytes()
    diagnostics['THIRD_PARTY_NOTICES.md'] = (ROOT / 'THIRD_PARTY_NOTICES.md').read_bytes()
    diagnostics['source-reference.json'] = (json.dumps(manifest['source_archive'],indent=2)+'\n').encode()
    diagnostics['SHA256SUMS'] = ''.join(sha(data)+'  '+name+'\n' for name,data in sorted(diagnostics.items())).encode()
    diagnostic_zip = OUTPUT_DIR / (NAME + '-diagnostic.zip')
    archive(diagnostic_zip,diagnostics)
    diagnostic_zip.with_suffix('.sha256').write_text(sha(diagnostic_zip.read_bytes())+'  '+diagnostic_zip.name+'\n',encoding='ascii')
    print(external, end='')


if __name__ == '__main__':
    main()
