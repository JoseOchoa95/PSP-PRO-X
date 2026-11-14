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
        phase3 = manifest.get('phase3')
        expected_psp = {'PSP/GAME/PROUPDATE/EBOOT.PBP', 'PSP/GAME/FastRecovery/EBOOT.PBP'}
        if phase3:
            expected_psp |= {'PSP/GAME/PROXUPDATE/EBOOT.PBP', 'PSP/GAME/PROXUPDATE/cacert.pem'}
        require(psp_names == expected_psp, 'Unexpected PSP program')
        require(not any(PurePosixPath(n).suffix.lower() in {'.prx', '.bin', '.a'} for n in names), 'Unexpected loose firmware/tool binary')
        for folder, title in [('PROUPDATE', '661 PRO-X Installer'), ('FastRecovery', '661 PRO-X Fast Recovery')]:
            if phase3:
                title = '661 PRO-X v'+manifest['version']+(' Installer' if folder == 'PROUPDATE' else ' Fast Recovery')
            require(pbp_info(archive.read('PSP/GAME/' + folder + '/EBOOT.PBP'))['title'] == title, 'Wrong title')
        if phase3:
            title = pbp_info(archive.read('PSP/GAME/PROXUPDATE/EBOOT.PBP'), allow_plain=True)['title']
            require(title == 'PRO-X v'+manifest['version']+' System Update', 'Wrong client title')
            require(manifest['hardware'] == 'NOT RUN' and manifest['status'] == 'phase3-update-test-candidate' and
                    phase3['online_installation_implemented'] is True and phase3['physical_validation'] == 'NOT RUN for this candidate',
                    'Misleading updater candidate status')
        require(manifest['firmware_target'] == '6.61' and manifest['hardware'] in
                {'NOT RUN', 'USER_REPORTED_PARTIAL_PASS'}, 'Invalid release status')
        source = path.parent / manifest['source_archive']['file']
        require(sha(source.read_bytes()) == manifest['source_archive']['sha256'], 'Companion source ZIP mismatch')
        with zipfile.ZipFile(source) as source_zip:
            snapshot = json.loads(source_zip.read('source-snapshot.json'))
            require(set(source_zip.namelist()) == set(snapshot['files']) | {'source-snapshot.json'}, 'Source ZIP inventory')
            for name, digest in snapshot['files'].items():
                require(sha(source_zip.read(name)) == digest, 'Source ZIP hash: ' + name)
            if manifest.get('wpa2'):
                require('firmware/tools/wpa2diag/main.c' in snapshot['files'], 'Diagnostic source omitted')
    external = dict((name, digest) for digest, name in (line.split('  ', 1) for line in path.with_suffix('.sha256').read_text().splitlines()))
    for file in [path, source]:
        require(sha(file.read_bytes()) == external[file.name], 'External ZIP hash mismatch')
    if phase3:
        compact_path = path.with_name(path.stem+'-build.json')
        compact = json.loads(compact_path.read_text())
        require(compact['build_id'] == manifest['build_id'] and compact['hardware'] == 'NOT RUN',
                'Compact manifest identity/status mismatch')
        update_path=path.with_name(path.stem+'-update.zip')
        update_json=path.parent/'pro-x-update.json'
        require(compact['packages'] == {p.name: sha(p.read_bytes()) for p in [path, source, update_path, update_json]},
                'Compact package hashes differ')
        from update_contract import FILES
        contract=json.loads(update_json.read_bytes())
        require(contract['version'] == manifest['version'] and contract['build_id'] == manifest['build_id'] and
                contract['schema_version'] == 1 and contract['supported_models'] == [1] and
                contract['minimum_updater_version'] == '0.3.0', 'Update contract identity/compatibility differs')
        require(contract['package_asset'] == {'name': update_path.name, 'size': update_path.stat().st_size,
                                              'sha256': sha(update_path.read_bytes())}, 'Update ZIP differs from contract')
        with zipfile.ZipFile(update_path) as ota:
            require(len(ota.namelist()) == len(FILES) and set(ota.namelist()) == set(FILES), 'Unexpected/duplicate update file')
            by_name={f['path']: f for f in contract['files']}
            require(len(contract['files']) == len(FILES) and set(by_name) == set(FILES), 'Update manifest inventory')
            for i,name in enumerate(FILES):
                data=ota.read(name)
                require(ota.getinfo(name).compress_type == zipfile.ZIP_STORED and
                        by_name[name] == {'path': name, 'size': len(data), 'sha256': sha(data)}, 'Update entry hash/size/format')
                with zipfile.ZipFile(path) as manual:
                    require(data == manual.read(('PSP/GAME/' if i < 4 else '')+name), 'Update/manual payload mismatch')
        for name, value in external.items():
            require(sha((path.parent/name).read_bytes()) == value, 'Delivered file hash mismatch: '+name)
        for name, info in compact['executables'].items():
            data = (path.parent/name).read_bytes()
            require(sha(data) == info['sha256'] and len(data) == info['size'], 'Delivered EBOOT differs: '+name)
    if manifest.get('wpa2'):
        if phase3:
            from product import VERSION, WPA2_VARIANT, WPA2_REPORT_SHA256
            require(manifest['version'] == VERSION, 'Milestone product version differs from compiled ticket')
            require(manifest['wpa2'] == {'preference_enabled_by_default': True, 'verified_variants': 1,
                    'variant_id': WPA2_VARIANT, 'identification_report_sha256': WPA2_REPORT_SHA256,
                    'patch_active': True, 'status': 'INHERITED_PARTIAL_REPORT_NEW_BUILD_NOT_RUN'},
                    'Misleading milestone WPA2 validation')
        elif manifest['status'] == 'blocked-module-identification':
            require(manifest['wpa2'] == {'preference_enabled_by_default': True, 'verified_variants': 0,
                                       'patch_active': False, 'status': 'BLOCKED'}, 'Misleading WPA2 status')
        else:
            from product import VERSION, WPA2_VARIANT, WPA2_REPORT_SHA256
            require(manifest['version'] == VERSION and manifest['status'] in
                    {'prepared-for-hardware-test','experimental-user-tested'}, 'Unexpected candidate status')
            reported = manifest['status'] == 'experimental-user-tested'
            require(manifest['wpa2'] == {'preference_enabled_by_default': True, 'verified_variants': 1,
                    'variant_id': WPA2_VARIANT, 'identification_report_sha256': WPA2_REPORT_SHA256,
                    'patch_active': True, 'status': 'CONNECTION_PASS_REPORTED' if reported else 'HARDWARE NOT RUN'}, 'Misleading candidate WPA2 status')
            require(bool(manifest.get('build_id')), 'Missing test build identity')
            if reported:
                report = manifest.get('hardware_report', {})
                require(manifest['hardware'] == 'USER_REPORTED_PARTIAL_PASS' and
                        report.get('model') == 'PSP-2010' and report.get('network_reported', {}).get('security') == 'WPA2 only',
                        'Missing bounded hardware report')
                require(report.get('transfer_5_minutes', '').startswith('NOT RUN') and
                        report.get('infinity', '').startswith('NOT RUN'), 'Unreported tests marked passed')
        diagnostic = path.with_name(path.stem + '-diagnostic.zip')
        with zipfile.ZipFile(diagnostic) as diagnostic_zip:
            sums = dict((name, value) for value, name in
                        (line.split('  ',1) for line in diagnostic_zip.read('SHA256SUMS').decode().splitlines()))
            require(len(diagnostic_zip.namelist()) == len(set(diagnostic_zip.namelist())), 'Duplicate diagnostic entry')
            require(set(diagnostic_zip.namelist()) == set(sums) | {'SHA256SUMS'}, 'Diagnostic inventory')
            for name, value in sums.items():
                require(sha(diagnostic_zip.read(name)) == value, 'Diagnostic hash: ' + name)
            title = pbp_info(diagnostic_zip.read('PSP/GAME/WPA2DIAG/EBOOT.PBP'),allow_plain=True)['title']
            require(title == 'PRO-X WPA2 identification (read only)', 'Diagnostic title')
            require(json.loads(diagnostic_zip.read('source-reference.json')) == manifest['source_archive'], 'Diagnostic source mismatch')
        expected_hash, expected_name = diagnostic.with_suffix('.sha256').read_text().strip().split('  ',1)
        require(expected_name == diagnostic.name and sha(diagnostic.read_bytes()) == expected_hash, 'Diagnostic external hash')
    print('PASS: ZIP inventory, payload/SFO, SHA-256, companion source and separate diagnostic')


if __name__ == '__main__':
    main()
