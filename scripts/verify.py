"""Offline checks of source scope, PBP titles and embedded module provenance."""
from pathlib import Path
import gzip
import json
import re
import struct
from build import ROOT, inventory, digest, sha
from product import VERSION, WPA2_VARIANT

REPLACEMENTS = {
    'Installer/main.c': [(b'#define VERSION_STR "PRO-C"', b'#define VERSION_STR "PRO-X"'),
                         (b'printf(VERSION_STR " by Team PRO\\n");', b'printf(VERSION_STR " - based on PRO-C by Team PRO\\n");')],
    'PXE/Launcher/Makefile': [(b'PRO-C Updater', b'PRO-X Installer')],
    'FastRecovery/Makefile': [(b'PRO-C Fast Recovery', b'PRO-X Fast Recovery')],
    'Vshctrl/vshctrl_patch_offset.c': [(b'"6.61 PRO-%c"', b'"6.61 PRO-X"')],
    'Satelite/trans_en.c': [(b'"PRO VSH MENU"', b'"PRO-X VSH MENU"')],
    'Recovery/trans_en.c': [(b'"PRO Recovery Menu"', b'"PRO-X Recovery Menu"')],
}

PHASE2_MODIFIED = {'Recovery/Makefile', 'Recovery/menu.c', 'SystemControl/Makefile',
                   'SystemControl/syspatch.c'}
PHASE2_ADDED = {'Common/prox_network.c', 'Common/prox_network.h',
                'Common/wpa2_patch.c', 'Common/wpa2_patch.h',
                'SystemControl/wpa2.c', 'SystemControl/wpa2.h', 'SystemControl/wpa2_variant_661.h',
                'tools/wpa2diag/Makefile', 'tools/wpa2diag/main.c'}

PHASE3_MODIFIED = {'Installer/main.c', 'Installer/Makefile', 'PXE/Launcher/main.c', 'SystemControl/Makefile', 'SystemControl/exports.exp',
                   'Vshctrl/custom_update.c', 'Vshctrl/exports.exp', 'Vshctrl/vshpatch.c',
                   'PXE/Launcher/Makefile', 'FastRecovery/Makefile', 'Vshctrl/Makefile'}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify_source(reference):
    actual = inventory(ROOT / 'firmware')
    phase2 = json.loads((ROOT / 'docs/phase-2-source.json').read_text())
    phase3 = json.loads((ROOT / 'scripts/phase-3-source.json').read_text())
    require(set(phase3['modified']) == PHASE3_MODIFIED, 'Phase 3 modified its reviewed source scope')
    require(set(phase2['modified']) == PHASE2_MODIFIED and set(phase2['added']) == PHASE2_ADDED,
            'Phase 2 changed its authorized file scope')
    require(set(actual) == set(reference['source_files']) | set(phase2['added']) | set(phase3['added']),
            'Unexpected/missing firmware files')
    changes = []
    for name, value in actual.items():
        data = (ROOT / 'firmware' / name).read_bytes()
        if name in phase3['added'] or name in phase3['modified']:
            require(value == phase3['added'].get(name, phase3['modified'].get(name)),
                    'Phase 3 reviewed source changed: ' + name)
            changes.append(name)
            continue
        if name in phase2['added'] or name in phase2['modified']:
            expected = phase2['added'].get(name, phase2['modified'].get(name))
            require(value == expected, 'Phase 2 reviewed snapshot changed: ' + name)
            changes.append(name)
            continue
        if name in REPLACEMENTS:
            for old, new in REPLACEMENTS[name]:
                require(data.count(new) == 1, 'Branding missing/duplicated: ' + name)
                data = data.replace(new, old)
            changes.append(name)
        elif name.startswith('Translated/recovery_') or name.startswith('Translated/satelite_'):
            index = 0 if '/recovery_' in name else 18
            lines = data.splitlines(keepends=True)
            require(lines[index].count(b'PRO-X') == 1, 'Translation title: ' + name)
            lines[index] = lines[index].replace(b'PRO-X', b'PRO')
            data = b''.join(lines)
            changes.append(name)
        require(sha(data) == reference['source_files'][name], 'Out-of-scope change: ' + name)
    require(len('6.61 PRO-X '+VERSION) + 1 <= 64, 'Dedicated version-label buffer limit')
    require('prox_system_version_patch((void *)text_addr, mod->text_size,' in
            (ROOT / 'firmware/Vshctrl/vshpatch.c').read_text(), 'Dedicated 6.61 label hook missing')
    # Explicitly verify additive exports and unchanged existing contracts.
    exports = (ROOT / 'firmware/SystemControl/exports.exp').read_bytes()
    extra = (b'PSP_EXPORT_START(ProxUpdateForUser, 0, 0x4001)\n'
             b'PSP_EXPORT_FUNC(proxUpdateGetState)\nPSP_EXPORT_FUNC(proxUpdateRandom)\nPSP_EXPORT_END\n\n')
    if b'\r\n' in exports:
        extra = extra.replace(b'\n', b'\r\n')
    require(exports.count(extra) == 1 and sha(exports.replace(extra, b'')) ==
            reference['source_files']['SystemControl/exports.exp'], 'Existing SystemCtrl exports changed')
    exports = (ROOT / 'firmware/Vshctrl/exports.exp').read_bytes()
    line = b'PSP_EXPORT_FUNC(proxInternetUpdate)' + (b'\r\n' if b'\r\n' in exports else b'\n')
    require(sha(exports.replace(line, b'')) ==
            reference['source_files']['Vshctrl/exports.exp'], 'Existing VSH exports changed')
    makefile = (ROOT / 'firmware/SystemControl/Makefile').read_bytes()
    line = b'\t   prox_update.o \\' + (b'\r\n' if b'\r\n' in makefile else b'\n')
    require(sha(makefile.replace(line, b'')) == phase2['modified']['SystemControl/Makefile'],
            'Unrelated SystemControl build contract changed')
    return sorted(changes)


def pbp_info(data, allow_plain=False):
    require(len(data) >= 40 and data[:4] == b'\0PBP', 'Invalid PBP magic/header')
    version, *offsets = struct.unpack_from('<9I', data, 4)
    require(version == 0x10000, 'Unexpected PBP version')
    require(offsets[0] == 40 and offsets == sorted(offsets) and offsets[-1] <= len(data), 'Invalid PBP offsets')
    sfo = data[offsets[0]:offsets[1]]
    require(sfo[:4] == b'\0PSF', 'Invalid PARAM.SFO')
    _, keys, values, count = struct.unpack_from('<4I', sfo, 4)
    entries = {}
    for i in range(count):
        key, fmt, length, capacity, offset = struct.unpack_from('<HHIII', sfo, 20 + i * 16)
        require(length <= capacity and values + offset + capacity <= len(sfo), 'SFO bounds')
        name = sfo[keys + key:].split(b'\0', 1)[0].decode()
        value = sfo[values + offset:values + offset + length]
        entries[name] = value.rstrip(b'\0').decode() if fmt == 0x204 else value.hex()
    payload = data[offsets[6]:offsets[7]]
    if payload[:4] == b'~PSP':
        require(struct.unpack_from('<I', payload, 0x2c)[0] == len(payload), 'PSP payload size mismatch')
        module = payload[10:38].split(b'\0', 1)[0].decode()
    else:
        require(allow_plain and payload[:4] == b'\x7fELF', 'PBP has no encrypted PSP payload')
        module = 'ELF diagnostic (CFW required)'
    return {'title': entries['TITLE'], 'category': entries['CATEGORY'],
            'module': module, 'sha256': sha(data)}


def header_bytes(path):
    data = path.read_text()
    array = data.split('= {', 1)[1].split('};', 1)[0]
    return bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', array))


def elf_data(path):
    data = path.read_bytes()
    return gzip.decompress(data[0x150:]) if data.startswith(b'~PSP') else data


def verify_build(metadata):
    output = ROOT / metadata['output']
    require(inventory(output / 'PSP') == metadata['artifacts'], 'PBP files changed after build')
    require(inventory(output / 'modules') == metadata['modules'], 'Modules changed after build')
    tree = output / 'build-tree'
    embedded = {}
    modules = ['ISODrivers/Galaxy/galaxy', 'ISODrivers/March33/march33',
               'ISODrivers/March33/march33_620', 'ISODrivers/March33/march33_660',
               'ISODrivers/Inferno/inferno', 'Popcorn/popcorn', 'Satelite/satelite',
               'Stargate/stargate', 'SystemControl/systemctrl', 'usbdevice/usbdevice',
               'Vshctrl/vshctrl', 'Recovery/recovery']
    installer = (tree / 'Installer/installer.prx').read_bytes()
    require(len(installer) <= 1024*1024, 'Installer exceeds launcher decompression buffer')
    for module in modules:
        path = Path(module)
        binary = (tree / (module + '.prx')).read_bytes()
        require(header_bytes(tree / 'Installer' / (path.name + '.h')) == binary, 'Embedded header differs: ' + module)
        if path.name not in ['march33', 'march33_620']:
            require(binary in installer, 'Module absent from installer: ' + module)
            embedded[module + '.prx'] = sha(binary)
    for name, original in [('installer', 'Installer/installer.prx'), ('Rebootex_prx', 'Rebootex/Rebootex.prx')]:
        require(gzip.decompress(header_bytes(tree / 'PXE/Launcher' / (name + '.h'))) == (tree / original).read_bytes(), 'Launcher payload mismatch: ' + name)
        require(header_bytes(tree / 'PXE/Launcher' / (name + '.h')) in
                (tree / 'PXE/Launcher/pxe_launcher.elf').read_bytes(), 'Current payload absent from compiled launcher: '+name)
    require(header_bytes(tree / 'PXE/Launcher/rebootex.h') == (tree / 'PXE/RebootexPXE/rebootex.bin').read_bytes(), 'PXE reboot mismatch')
    require(header_bytes(tree / 'PXE/RebootexPXE/systemctrl.h') == (tree / 'PXE/RebootexPXE/systemctrl.prx').read_bytes(), 'PXE bootstrap mismatch')
    reboot = (tree / 'Rebootex_bin/rebootex.bin').read_bytes()
    require(header_bytes(tree / 'Rebootex_bin/rebootex.h') == reboot, 'Live reboot header mismatch')
    require(reboot in (tree / 'FastRecovery/fast_recovery.elf').read_bytes(), 'Fast Recovery reboot mismatch')
    require(reboot in (tree / 'Rebootex/Rebootex.prx').read_bytes(), 'Installer live reboot mismatch')
    titles = [('PROUPDATE', '661 PRO-X v'+VERSION+' Installer', '661 PRO-C Updater'),
              ('FastRecovery', '661 PRO-X v'+VERSION+' Fast Recovery', '661 PRO-C Fast Recovery')]
    info = {}
    for directory, branded, original in titles:
        info[directory] = pbp_info((output / 'PSP/GAME' / directory / 'EBOOT.PBP').read_bytes())
        require(info[directory]['title'] == (branded if metadata['mode'] == 'pro-x' else original), 'Wrong EBOOT title')
        require(info[directory]['category'] == 'MG', 'Wrong EBOOT category')
    if metadata['mode'] == 'pro-x':
        for file, message in [('Vshctrl/vshctrl.prx', ('6.61 PRO-X '+VERSION).encode()), ('Recovery/recovery.prx', b'PRO-X Recovery Menu'), ('Satelite/satelite.prx', b'PRO-X VSH MENU'), ('Installer/installer.prx', ('PRO-X v'+VERSION+' - based on PRO-C by Team PRO').encode())]:
            require(message in elf_data(tree / file), 'Visible text absent from generated module: ' + file)
        for name in ['wpa2.o', '../Common/wpa2_patch.o', '../Common/prox_network.o']:
            require((tree / 'SystemControl' / name).is_file(), 'Phase 2 object missing: ' + name)
        require(b'prox_wpa2_module' in (tree / 'SystemControl/systemctrl.elf').read_bytes(), 'WPA2 dispatcher not linked')
        require(inventory(output / 'diagnostics') == metadata['diagnostics'], 'Diagnostic changed since build')
        require(bool(metadata['diagnostics']), 'Diagnostic not built')
        diagnostic = pbp_info((output / 'diagnostics/PSP/GAME/WPA2DIAG/EBOOT.PBP').read_bytes(), allow_plain=True)
        require(diagnostic['title'] == 'PRO-X WPA2 identification (read only)', 'Wrong diagnostic title')
        client = pbp_info((output / 'PSP/GAME/PROXUPDATE/EBOOT.PBP').read_bytes(), allow_plain=True)
        require(client['title'] == 'PRO-X v'+VERSION+' System Update', 'Wrong updater product title')
        require(metadata['build_id'].encode() in (tree / 'Updater/prox_updater.elf').read_bytes(), 'Updater build ID missing')
        require(b'ProxUpdateForUser' not in (tree / 'Installer/installer.elf').read_bytes(),
                'Manual installer must not require the private updater bridge')
        require(b'ProxUpdateForUser' in (tree / 'SystemControl/systemctrl.elf').read_bytes(), 'Private bridge missing')
        deps = json.loads((ROOT / 'scripts/updater-dependencies.json').read_text())
        for dependency in ['mbedtls', 'ca_bundle']:
            require(sha((ROOT / deps[dependency]['path']).read_bytes()) == deps[dependency]['sha256'],
                    'Pinned updater dependency changed: '+dependency)
        require((output / 'PSP/GAME/PROXUPDATE/cacert.pem').read_bytes() ==
                (ROOT / deps['ca_bundle']['path']).read_bytes(), 'Packaged CA bundle changed')
        info['PROXUPDATE'] = client
    return {'pbp': info, 'embedded_modules': embedded}


def main():
    reference = json.loads((ROOT / 'artifacts/reference-build.json').read_text())
    branded = json.loads((ROOT / 'artifacts/pro-x-build.json').read_text())
    require(branded.get('product_version') == VERSION, 'Compiled product version mismatch')
    require(reference['toolchain_image'] == branded['toolchain_image'], 'Different toolchains')
    for build in [reference, branded]:
        require(build['compile_script_sha256'] == sha((ROOT / 'scripts/compile.sh').read_bytes()), 'Build script changed')
        require(build['dockerfile_sha256'] == sha((ROOT / 'scripts/Dockerfile').read_bytes()), 'Dockerfile changed')
    changes = verify_source(reference)
    require(inventory(ROOT / 'firmware') == branded['source_files'], 'Sources changed since PRO-X compilation')
    results = {'source_scope': 'PASS', 'branding_files': changes,
               'contracts': 'PASS: reviewed phase 2/3 scope; existing PRO exports/NIDs/config/HEN/boot paths preserved; private updater library and VSH syscall added',
               'phase2': {'status': 'prepared-for-hardware-test', 'verified_variants': 1,
                          'variant_id': WPA2_VARIANT, 'preference_default': 'enabled',
                          'hardware': 'NOT RUN'},
               'reference': verify_build(reference), 'pro-x': verify_build(branded),
               'utf16_display_bytes_including_nul': len(('6.61 PRO-X '+VERSION).encode('utf-16le'))+2,
               'display_storage': 'separately sized user buffer; Sony string capacity not assumed',
               'hardware': 'NOT RUN',
               'infinity': 'NOT RUN: static contract preservation only'}
    (ROOT / 'artifacts/verification.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print('PASS: source scope, HEN/ABI/paths preservation, PBP/SFO, visible resources, embedded modules and shared reboot buffers')


if __name__ == '__main__':
    main()
