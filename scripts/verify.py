"""Offline checks of source scope, PBP titles and embedded module provenance."""
from pathlib import Path
import gzip
import json
import re
import struct
from build import ROOT, inventory, digest, sha

REPLACEMENTS = {
    'Installer/main.c': [(b'#define VERSION_STR "PRO-C"', b'#define VERSION_STR "PRO-X"'),
                         (b'printf(VERSION_STR " by Team PRO\\n");', b'printf(VERSION_STR " - based on PRO-C by Team PRO\\n");')],
    'PXE/Launcher/Makefile': [(b'PRO-C Updater', b'PRO-X Installer')],
    'FastRecovery/Makefile': [(b'PRO-C Fast Recovery', b'PRO-X Fast Recovery')],
    'Vshctrl/vshctrl_patch_offset.c': [(b'"6.61 PRO-%c"', b'"6.61 PRO-X"')],
    'Satelite/trans_en.c': [(b'"PRO VSH MENU"', b'"PRO-X VSH MENU"')],
    'Recovery/trans_en.c': [(b'"PRO Recovery Menu"', b'"PRO-X Recovery Menu"')],
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify_source(reference):
    actual = inventory(ROOT / 'firmware')
    require(actual.keys() == reference['source_files'].keys(), 'Unexpected/missing firmware files')
    changes = []
    for name, value in actual.items():
        data = (ROOT / 'firmware' / name).read_bytes()
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
    require(len('6.61 PRO-X'.encode('utf-16le')) == len('6.61 PRO-C'.encode('utf-16le')), 'UTF-16 growth')
    require(len('6.61 PRO-X') + 1 <= 30, 'ASCII buffer overflow')
    return sorted(changes)


def pbp_info(data):
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
    require(payload[:4] == b'~PSP', 'PBP has no encrypted PSP payload')
    require(struct.unpack_from('<I', payload, 0x2c)[0] == len(payload), 'PSP payload size mismatch')
    return {'title': entries['TITLE'], 'category': entries['CATEGORY'],
            'module': payload[10:38].split(b'\0', 1)[0].decode(), 'sha256': sha(data)}


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
    for module in modules:
        path = Path(module)
        binary = (tree / (module + '.prx')).read_bytes()
        require(header_bytes(tree / 'Installer' / (path.name + '.h')) == binary, 'Embedded header differs: ' + module)
        if path.name not in ['march33', 'march33_620']:
            require(binary in installer, 'Module absent from installer: ' + module)
            embedded[module + '.prx'] = sha(binary)
    for name, original in [('installer', 'Installer/installer.prx'), ('Rebootex_prx', 'Rebootex/Rebootex.prx')]:
        require(gzip.decompress(header_bytes(tree / 'PXE/Launcher' / (name + '.h'))) == (tree / original).read_bytes(), 'Launcher payload mismatch: ' + name)
    require(header_bytes(tree / 'PXE/Launcher/rebootex.h') == (tree / 'PXE/RebootexPXE/rebootex.bin').read_bytes(), 'PXE reboot mismatch')
    require(header_bytes(tree / 'PXE/RebootexPXE/systemctrl.h') == (tree / 'PXE/RebootexPXE/systemctrl.prx').read_bytes(), 'PXE bootstrap mismatch')
    reboot = (tree / 'Rebootex_bin/rebootex.bin').read_bytes()
    require(header_bytes(tree / 'Rebootex_bin/rebootex.h') == reboot, 'Live reboot header mismatch')
    require(reboot in (tree / 'FastRecovery/fast_recovery.elf').read_bytes(), 'Fast Recovery reboot mismatch')
    require(reboot in (tree / 'Rebootex/Rebootex.prx').read_bytes(), 'Installer live reboot mismatch')
    titles = [('PROUPDATE', '661 PRO-X Installer', '661 PRO-C Updater'),
              ('FastRecovery', '661 PRO-X Fast Recovery', '661 PRO-C Fast Recovery')]
    info = {}
    for directory, branded, original in titles:
        info[directory] = pbp_info((output / 'PSP/GAME' / directory / 'EBOOT.PBP').read_bytes())
        require(info[directory]['title'] == (branded if metadata['mode'] == 'pro-x' else original), 'Wrong EBOOT title')
        require(info[directory]['category'] == 'MG', 'Wrong EBOOT category')
    if metadata['mode'] == 'pro-x':
        for file, message in [('Vshctrl/vshctrl.prx', b'6.61 PRO-X'), ('Recovery/recovery.prx', b'PRO-X Recovery Menu'), ('Satelite/satelite.prx', b'PRO-X VSH MENU'), ('Installer/installer.prx', b'PRO-X - based on PRO-C by Team PRO')]:
            require(message in elf_data(tree / file), 'Visible text absent from generated module: ' + file)
    return {'pbp': info, 'embedded_modules': embedded}


def main():
    reference = json.loads((ROOT / 'artifacts/reference-build.json').read_text())
    branded = json.loads((ROOT / 'artifacts/pro-x-build.json').read_text())
    require(reference['toolchain_image'] == branded['toolchain_image'], 'Different toolchains')
    for build in [reference, branded]:
        require(build['compile_script_sha256'] == sha((ROOT / 'scripts/compile.sh').read_bytes()), 'Build script changed')
        require(build['dockerfile_sha256'] == sha((ROOT / 'scripts/Dockerfile').read_bytes()), 'Dockerfile changed')
    changes = verify_source(reference)
    require(inventory(ROOT / 'firmware') == branded['source_files'], 'Sources changed since PRO-X compilation')
    results = {'source_scope': 'PASS', 'branding_files': changes,
               'contracts': 'PASS: all firmware files outside the explicit visible text changes match reference byte for byte',
               'reference': verify_build(reference), 'pro-x': verify_build(branded),
               'utf16_display_bytes_including_nul': 22, 'hardware': 'NOT RUN',
               'infinity': 'NOT RUN: static contract preservation only'}
    (ROOT / 'artifacts/verification.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print('PASS: source scope, HEN/ABI/paths preservation, PBP/SFO, visible resources, embedded modules and shared reboot buffers')


if __name__ == '__main__':
    main()
