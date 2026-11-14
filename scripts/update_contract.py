"""Generate the exact schema-1 OTA assets; never publish or classify releases."""
import json
import zipfile
from build import sha

FILES = ('PROUPDATE/EBOOT.PBP', 'FastRecovery/EBOOT.PBP', 'PROXUPDATE/EBOOT.PBP',
         'PROXUPDATE/cacert.pem', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'SOURCE.md',
         'licenses/mbedtls/LICENSE')

def generate(output, version, build_id, manual_files):
    payload = {name: manual_files[('PSP/GAME/' if i < 4 else '')+name]
               for i, name in enumerate(FILES)}
    path = output / ('pro-x-'+version+'-661-update.zip')
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_STORED) as archive:
        for name, data in sorted(payload.items()):
            entry = zipfile.ZipInfo(name, (2026,11,13,0,0,0))
            entry.compress_type = zipfile.ZIP_STORED
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry, data)
    contract = {
        'schema_version': 1, 'product': 'PRO-X', 'repository': 'JoseOchoa95/PSP-PRO-X',
        'version': version, 'tag': 'v'+version, 'channel': 'stable', 'firmware_target': '6.61',
        # Only the owner-tested model ID from the physical transport evidence.
        # This declaration still needs the positive OTA installation test.
        'supported_models': [1], 'minimum_updater_version': '0.3.0', 'build_id': build_id,
        'package_asset': {'name': path.name, 'size': path.stat().st_size, 'sha256': sha(path.read_bytes())},
        'files': [{'path': name, 'size': len(data), 'sha256': sha(data)} for name,data in sorted(payload.items())],
        'summary': 'PRO-X '+version+' for firmware 6.61. Review the release notes before installing.',
    }
    manifest = output/'pro-x-update.json'
    manifest.write_bytes((json.dumps(contract, indent=2)+'\n').encode('utf-8'))
    return path, manifest
