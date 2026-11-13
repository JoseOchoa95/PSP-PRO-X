"""Create an explicit experimental payload and companion build-source archive."""
from pathlib import Path
import json
import zipfile
import subprocess
import sys
from build import ROOT, inventory, digest, sha
from verify import require

NAME = 'pro-x-0.1.0-alpha.1-661'
SOURCE_DIRS = {'Common', 'CrossFW', 'FastRecovery', 'Imports', 'Installer',
               'ISODrivers', 'Popcorn', 'PXE', 'Rebootex', 'Rebootex_bin',
               'Recovery', 'Satelite', 'Stargate', 'SystemControl', 'Translated',
               'Vshctrl', 'include', 'libs', 'usbdevice'}


def archive(path, files):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as stream:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, date_time=(2026, 11, 13, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            stream.writestr(entry, data)


def main():
    subprocess.run([sys.executable, str(ROOT / 'scripts/verify.py')], check=True, cwd=ROOT)
    build = json.loads((ROOT / 'artifacts/pro-x-build.json').read_text())
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
        'PRO-X 0.1.0-alpha.1 / 6.61: experimental; hardware NOT RUN.\n\n'
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
        'repository_head': build['repository_head'],
        'files': source_hashes, 'complete_build_inputs': True, 'complete_upstream_tree': False,
    }, indent=2) + '\n').encode()
    source_zip = ROOT / 'artifacts' / (NAME + '-source.zip')
    archive(source_zip, sources)

    files = {}
    output = ROOT / build['output']
    for directory in ['PROUPDATE', 'FastRecovery']:
        name = 'PSP/GAME/' + directory + '/EBOOT.PBP'
        files[name] = (output / name).read_bytes()
    for name in inventory(ROOT / 'firmware/Translated'):
        files['seplugins/' + name] = (ROOT / 'firmware/Translated' / name).read_bytes()
    for name in inventory(ROOT / 'firmware/contrib/fonts'):
        files['seplugins/fonts/' + name] = (ROOT / 'firmware/contrib/fonts' / name).read_bytes()
    for name in ['LICENSE', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md']:
        files[name] = (ROOT / name).read_bytes()
    for name in ['build.md', 'upstream.md', 'install-and-validation.md', 'phase-1-results.md']:
        files['docs/' + name] = (ROOT / 'docs' / name).read_bytes()
    for name in inventory(ROOT / 'docs/licenses'):
        files['licenses/' + name] = (ROOT / 'docs/licenses' / name).read_bytes()
    files['credit.txt'] = (ROOT / 'firmware/credit.txt').read_bytes()
    files['SOURCE.md'] = source_note + ('\nCompanion: ' + source_zip.name + '\nSHA-256: ' + sha(source_zip.read_bytes()) + '\n').encode()
    files['toolchain.txt'] = (ROOT / 'artifacts/toolchain.txt').read_bytes()
    manifest = {
        'product': 'PRO-X', 'version': '0.1.0-alpha.1', 'firmware_target': '6.61',
        'status': 'implementation-prepared-for-testing', 'hardware': 'NOT RUN', 'infinity': 'NOT RUN',
        'upstream_url': 'https://github.com/MrColdbird/procfw',
        'upstream_revision': build['upstream_revision'], 'repository_head': build['repository_head'],
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
    binary_zip = ROOT / 'artifacts' / (NAME + '.zip')
    archive(binary_zip, files)
    external = ''.join(sha(path.read_bytes()) + '  ' + path.name + '\n' for path in [binary_zip, source_zip])
    (ROOT / 'artifacts' / (NAME + '.sha256')).write_text(external, encoding='ascii')
    (ROOT / 'artifacts/manifest.json').write_bytes(files['manifest.json'])
    print(external, end='')


if __name__ == '__main__':
    main()
