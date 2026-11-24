"""Generate a data-only update PBP from the exact resident build outputs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from product import VERSION

PARTS = (
    'SystemControl/systemctrl.prx',
    'Vshctrl/vshctrl.prx',
    'ISODrivers/Galaxy/galaxy.prx',
    'Stargate/stargate.prx',
    'ISODrivers/March33/march33_660.prx',
    'ISODrivers/Inferno/inferno.prx',
    'usbdevice/usbdevice.prx',
    'Popcorn/popcorn.prx',
    'Satelite/satelite.prx',
    'Recovery/recovery.prx',
    'Rebootex/Rebootex.prx',
    'Updater/prox_updater.prx',
)
ENGINE_REVISION = 1
DEVKIT = 0x06060110


def param_sfo():
    keys, values, entries = bytearray(), bytearray(), bytearray()
    for key, value in ((b'CATEGORY\0', b'MG\0'), (b'TITLE\0', b'PRO-X Update Data\0')):
        capacity = (len(value) + 3) & ~3
        entries.extend(struct.pack('<HHIII', len(keys), 0x204, len(value), capacity, len(values)))
        keys.extend(key)
        values.extend(value.ljust(capacity, b'\0'))
    keys.extend(b'\0' * (-len(keys) % 4))
    return struct.pack('<4s4I', b'\0PSF', 0x101, 52, 52 + len(keys), 2) + entries + keys + values


def render_parts(parts, build_id, product_version):
    if not re.fullmatch(r'pro-x-[0-9a-f]{12}', build_id):
        raise ValueError('Invalid PRO-X build ID')
    version = tuple(map(int, product_version.split('.')))
    header_size = 64 + len(PARTS) * 48
    index, data, inventory = bytearray(), bytearray(), []
    parts = tuple(parts)
    if len(parts) != len(PARTS):
        raise ValueError('Wrong dedicated part count')
    for number, (path, image) in enumerate(zip(PARTS, parts)):
        if not 52 <= len(image) <= 1024 * 1024:
            raise ValueError('Part size out of bounds: ' + path)
        digest = hashlib.sha256(image).digest()
        offset = header_size + len(data)
        index.extend(struct.pack('<4I32s', number, offset, len(image), len(image), digest))
        data.extend(image)
        inventory.append(dict(id=number, source=path, size=len(image), sha256=digest.hex()))
    sfo = param_sfo()
    prefix = struct.pack('<8s8I20sI', b'PROXUPD\0', 1, header_size, DEVKIT, 2,
                         ENGINE_REVISION, *version, build_id.encode(), len(PARTS))
    pbp = struct.pack('<4s9I', b'\0PBP', 0x10000, 40, *([40 + len(sfo)] * 7))
    pbp += sfo + prefix + index + data
    if len(pbp) > 3 * 1024 * 1024:
        raise ValueError('PBP exceeds the dedicated reader limit')
    manifest = dict(format='prox-dedicated-eboot-v1', product_version=product_version, build_id=build_id,
                    target_firmware='6.61', models=[1], minimum_engine_revision=ENGINE_REVISION,
                    size=len(pbp), sha256=hashlib.sha256(pbp).hexdigest(), parts=inventory)
    return pbp, manifest


def render(tree, build_id):
    return render_parts(((tree / path).read_bytes() for path in PARTS), build_id, VERSION)


def release_manifest(identity):
    version = identity['product_version']
    return dict(schema_version=2, product='PRO-X', repository='JoseOchoa95/PSP-PRO-X',
                format=identity['format'], version=version, tag='v'+version, channel='stable',
                firmware_target='6.61', supported_models=identity['models'],
                minimum_updater_version='0.4.0', minimum_engine_revision=identity['minimum_engine_revision'],
                build_id=identity['build_id'], summary='PRO-X dedicated system update',
                package_asset=dict(name='EBOOT.PBP', size=identity['size'], sha256=identity['sha256']))


def render_fixture():
    names = ('SystemControl', 'VshCtrl', 'PROGalaxyController', 'stargate', 'pspMarch33_Driver',
             'PRO_Inferno_Driver', 'pspUsbDev_Driver', 'PROPopcornManager', 'VshCtrlSatelite',
             'Recovery', 'fast_recovery', 'ProxUpdater')
    parts = []
    for name in names:
        data = bytearray(8192)
        data[:4] = b'~PSP'
        data[10:10 + len(name)] = name.encode('ascii')
        struct.pack_into('<I', data, 0x2c, len(data))
        parts.append(data)
    major, minor, patch = map(int, VERSION.split('.'))
    return render_parts(parts, 'pro-x-000000000001', f'{major}.{minor}.{patch + 1}')


def verifier_pin(manifest):
    digest = ', '.join('0x%02x' % byte for byte in bytes.fromhex(manifest['sha256']))
    version = ', '.join(manifest['product_version'].split('.'))
    return (
        '/* Generated pin for the read-only verifier. */\n'
        'static const ProxDedicatedExpected fixture = {\n'
        '    .size = %du,\n'
        '    .sha256 = { %s },\n'
        '    .running = { 0, 0, 0 },\n'
        '    .target = { %s },\n'
        '    .build_id = "%s",\n'
        '    .devkit = 0x%08xu, .model = 1, .engine_revision = %du, .minimum_engine_revision = %du\n'
        '};\n'
    ) % (manifest['size'], digest, version, manifest['build_id'], DEVKIT, ENGINE_REVISION,
         manifest['minimum_engine_revision'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tree', type=Path)
    parser.add_argument('build_id')
    parser.add_argument('output', type=Path)
    parser.add_argument('--fixture', action='store_true')
    args = parser.parse_args()
    if args.fixture:
        pbp, manifest = render_fixture()
        args.output.mkdir(parents=True, exist_ok=False)
        (args.output / 'PROX-DEDICATED-TEST.PBP').write_bytes(pbp)
        (args.tree / 'Updater/dedicated_fixture.h').write_bytes((
            '#define PROX_FIXTURE_SIZE %du\n'
            '#define PROX_FIXTURE_SHA "%s"\n'
            '#define PROX_FIXTURE_MANIFEST_SHA "%s"\n'
            '#define PROX_FIXTURE_VERSION "%s"\n'
            '#define PROX_FIXTURE_BUILD "%s"\n'
        ).encode('ascii') % (manifest['size'], manifest['sha256'].encode(),
                            hashlib.sha256(json.dumps(release_manifest(manifest)).encode()).hexdigest().encode(),
                            manifest['product_version'].encode(), manifest['build_id'].encode()))
        return
    pbp, manifest = render(args.tree, args.build_id)
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / 'EBOOT.PBP').write_bytes(pbp)
    (args.output / 'dedicated-update.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode())
    (args.output / 'pro-x-dedicated-update.json').write_bytes((json.dumps(release_manifest(manifest), indent=2) + '\n').encode())
    (args.tree / 'tools/updateverify/expected_update.h').write_bytes(verifier_pin(manifest).encode('ascii'))


if __name__ == '__main__':
    main()
