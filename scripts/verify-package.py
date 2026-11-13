"""Reopen a package and verify its inventory, SHA-256 and both XMB titles."""
from pathlib import Path, PurePosixPath
import json
import sys
import zipfile
from build import sha
from verify import require, pbp_info


def main():
    path = Path(sys.argv[1])
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        require(len(names) == len(set(names)), 'Duplicate ZIP entries')
        require(all(not PurePosixPath(n).is_absolute() and '..' not in PurePosixPath(n).parts and '\\' not in n for n in names), 'Unsafe ZIP path')
        manifest = json.loads(archive.read('manifest.json'))
        expected = set(manifest['files']) | {'manifest.json', 'SHA256SUMS'}
        require(set(names) == expected, 'ZIP inventory differs from manifest')
        for name, digest in manifest['files'].items():
            require(sha(archive.read(name)) == digest, 'SHA mismatch: ' + name)
        sums = dict((name, digest) for digest, name in (line.split('  ', 1) for line in archive.read('SHA256SUMS').decode().splitlines()))
        require(set(sums) == set(names) - {'SHA256SUMS'}, 'SHA256SUMS inventory')
        for name, digest in sums.items():
            require(sha(archive.read(name)) == digest, 'SHA256SUMS mismatch: ' + name)
        psp_names = {n for n in names if n.startswith('PSP/')}
        require(psp_names == {'PSP/GAME/PROUPDATE/EBOOT.PBP', 'PSP/GAME/FastRecovery/EBOOT.PBP'}, 'Unexpected PSP program')
        require(not any(PurePosixPath(n).suffix.lower() in {'.prx', '.bin', '.a'} for n in names), 'Unexpected loose firmware/tool binary')
        for folder, title in [('PROUPDATE', '661 PRO-X Installer'), ('FastRecovery', '661 PRO-X Fast Recovery')]:
            require(pbp_info(archive.read('PSP/GAME/' + folder + '/EBOOT.PBP'))['title'] == title, 'Wrong title')
        require(manifest['firmware_target'] == '6.61' and manifest['hardware'] == 'NOT RUN', 'Invalid release status')
        source = path.parent / manifest['source_archive']['file']
        require(sha(source.read_bytes()) == manifest['source_archive']['sha256'], 'Companion source ZIP mismatch')
        with zipfile.ZipFile(source) as source_zip:
            snapshot = json.loads(source_zip.read('source-snapshot.json'))
            require(set(source_zip.namelist()) == set(snapshot['files']) | {'source-snapshot.json'}, 'Source ZIP inventory')
            for name, digest in snapshot['files'].items():
                require(sha(source_zip.read(name)) == digest, 'Source ZIP hash: ' + name)
    external = dict((name, digest) for digest, name in (line.split('  ', 1) for line in path.with_suffix('.sha256').read_text().splitlines()))
    for file in [path, source]:
        require(sha(file.read_bytes()) == external[file.name], 'External ZIP hash mismatch')
    print('PASS: ZIP inventory, payload/SFO, all SHA-256 hashes and companion source archive')


if __name__ == '__main__':
    main()
