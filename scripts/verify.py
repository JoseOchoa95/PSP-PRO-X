"""Offline checks of source scope, PBP titles and embedded module provenance."""
from pathlib import Path
import gzip
import hashlib
import json
import re
import struct
import zlib
from build import ROOT, inventory, digest, sha
from product import VERSION, WPA2_VARIANT
from dedicated_update import release_manifest, render as render_dedicated, render_fixture, verifier_pin
from native_signed_templates import render as render_signed_templates

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
                   'contrib/PrxEncrypter/main.c',
                   'Vshctrl/custom_update.c', 'Vshctrl/exports.exp', 'Vshctrl/vshpatch.c',
                   'PXE/Launcher/Makefile', 'FastRecovery/Makefile', 'Vshctrl/Makefile', 'Vshctrl/vshmenu.c'}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def elf_u32_symbol(data, name):
    """Read an initialized ELF32 symbol from the compiled PSP artifact."""
    require(data[:6] == b'\x7fELF\x01\x01', 'Expected little-endian ELF32')
    start = struct.unpack_from('<I', data, 32)[0]
    width, count = struct.unpack_from('<HH', data, 46)
    require(width == 40 and start + width * count <= len(data), 'Invalid ELF sections')
    sections = [struct.unpack_from('<10I', data, start+i*width) for i in range(count)]
    found = []
    for section in sections:
        if section[1] != 2:
            continue
        require(section[9] == 16 and section[6] < count, 'Invalid ELF symbol table')
        strings = sections[section[6]]
        names = data[strings[4]:strings[4]+strings[5]]
        for offset in range(section[4], section[4]+section[5], 16):
            label, address, size, _, _, index = struct.unpack_from('<IIIBBH', data, offset)
            if names[label:].split(b'\0', 1)[0] != name.encode():
                continue
            require(0 < index < count and size == 4, 'Invalid scalar symbol')
            target = sections[index]
            position = address-target[3]
            require(target[1] != 8 and 0 <= position <= target[5]-4, 'Uninitialized scalar symbol')
            found.append(struct.unpack_from('<I', data, target[4]+position)[0])
    require(len(found) == 1, 'Missing/ambiguous compiled symbol: '+name)
    return found[0]


def elf_function_frames(data,name):
    """Bound the actual MIPS syscall-entry frame, not its user thread stack."""
    require(data[:6]==b'\x7fELF\x01\x01','Expected ELF32')
    start=struct.unpack_from('<I',data,32)[0];width,count=struct.unpack_from('<HH',data,46)
    require(width==40,'Invalid ELF sections')
    sections=[struct.unpack_from('<10I',data,start+i*width) for i in range(count)];found=[]
    for section in sections:
        if section[1]!=2:continue
        names=sections[section[6]];strings=data[names[4]:names[4]+names[5]]
        for at in range(section[4],section[4]+section[5],16):
            label,address,size,_,_,index=struct.unpack_from('<IIIBBH',data,at)
            if strings[label:].split(b'\0',1)[0]!=name.encode():continue
            require(0<index<count and size and not size%4,'Invalid function symbol')
            target=sections[index];offset=target[4]+address-target[3]
            words=struct.unpack_from('<'+str(size//4)+'I',data,offset)
            found.append([0x10000-(word&0xffff) for word in words if word&0xffff0000==0x27bd0000 and word&0x8000])
    require(len(found)==1 and found[0],'Missing compiled entry/frame: '+name)
    return found[0]


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
        updater_elf = (tree / 'Updater/prox_updater.elf').read_bytes()
        require(metadata['build_id'].encode() in updater_elf, 'Updater build ID missing')
        require(b'ProxUpdateForUser' not in (tree / 'Installer/installer.elf').read_bytes(),
                'Manual installer must not require the private updater bridge')
        require(b'ProxUpdateForUser' in (tree / 'SystemControl/systemctrl.elf').read_bytes(), 'Private bridge missing')
        deps = json.loads((ROOT / 'scripts/updater-dependencies.json').read_text())
        for dependency in ['mbedtls', 'ca_bundle']:
            require(sha((ROOT / deps[dependency]['path']).read_bytes()) == deps[dependency]['sha256'],
                    'Pinned updater dependency changed: '+dependency)
        ca_bundle = (ROOT / deps['ca_bundle']['path']).read_bytes()
        dedicated_ota = metadata.get('resident_dedicated_ota',False)
        if metadata.get('resident_query'):
            installer_elf = (tree / 'Installer/installer.elf').read_bytes()
            require(all(marker in installer_elf for marker in (b'flash0:/kd/_proxc661.bin',
                    b'prox_resident_completion_prepare', b'prox_completion_store_valid',
                    b'4b47ddf778c8df121a7fe3ade8acaada307ffd3e96d17834f568e29240557ba7')),
                    'Resident Installer must verify original compact completion data before provisioning')
            require(set(metadata['artifacts']) == {'GAME/PROUPDATE/EBOOT.PBP', 'GAME/FastRecovery/EBOOT.PBP'},
                    'Resident diagnostic must ship exactly the Installer and Fast Recovery applications')
            if metadata.get('resident_dedicated_preview'):
                fixture, identity = render_fixture()
                require((output/'dedicated-fixture/PROX-DEDICATED-TEST.PBP').read_bytes() == fixture and
                        identity['sha256'].encode() in updater_elf and b'prox_native_dedicated_stage' in updater_elf
                        and b'Dedicated diagnostic stage removed; no firmware write attempted' in updater_elf and
                        b'Dedicated Settings writer handoff:' in updater_elf and
                        b'prox_dedicated_writer_check' in updater_elf and b'prox_dedicated_writer_prepare' not in updater_elf and
                        b'prox_dedicated_record_commit' not in updater_elf and b'prox_dedicated_cleanup_committed' not in updater_elf and
                        b'prox_native_background_launch' not in updater_elf,
                        'Dedicated diagnostic must pin the synthetic PBP and restrict the handoff to read-only checks')
            else:
                dedicated = output / 'dedicated-update'
                expected_pbp, expected_manifest = render_dedicated(tree, metadata['build_id'])
                require((dedicated / 'EBOOT.PBP').read_bytes() == expected_pbp and
                        json.loads((dedicated / 'dedicated-update.json').read_text()) == expected_manifest and
                        json.loads((dedicated / 'pro-x-dedicated-update.json').read_text()) == release_manifest(expected_manifest),
                        'Dedicated EBOOT must match the exact compiled source parts')
                require((tree / 'tools/updateverify/expected_update.h').read_bytes() == verifier_pin(expected_manifest).encode('ascii'),
                        'Dedicated EBOOT verifier pin differs from the generated data asset')
                probe = (output / 'diagnostics/PSP/GAME/PROXEBOOT/EBOOT.PBP').read_bytes()
                require(pbp_info(probe, allow_plain=True)['title'] == 'PRO-X Update File Check' and
                        probe == (tree / 'tools/updateverify/EBOOT.PBP').read_bytes() and
                        metadata['build_id'].encode() in probe and bytes.fromhex(expected_manifest['sha256']) in probe and
                        b'PASS: dedicated EBOOT and %u parts verified.' in probe and b'flash0:/' not in probe,
                        'Read-only verifier must pin the matching dedicated EBOOT')
            capacity_dir = tree / 'Installer/flash_capacity'
            capacity = (capacity_dir / 'flash_capacity.prx').read_bytes()
            capacity_elf = (capacity_dir / 'flash_capacity.elf').read_bytes()
            require(header_bytes(tree / 'Installer/flash_capacity_prx.h') == capacity and installer.count(capacity) == 1,
                    'Installer must embed the exact built flash0 capacity helper')
            require(b'\x00\x10\x00\x01ProxInstallerCapacity\0' in capacity and b'lflash0:0,0\0' in capacity and
                    all(name in capacity_elf for name in (b'sceIoIoctl\0', b'sceIoRead\0', b'sceIoClose\0')) and
                    not any(name in capacity_elf for name in (b'sceIoWrite\0', b'sceIoRemove\0', b'sceIoAssign\0',
                                                             b'sceIoUnassign\0', b'sceIoDevctl\0', b'sceKernelExitGame\0')),
                    'Capacity helper must be a read-only kernel module')
            embedded['Installer/flash_capacity/flash_capacity.prx'] = sha(capacity)
            worker = (tree / 'Updater/prox_updater.prx').read_bytes()
            worker_sha = sha(worker)
            path = 'flash0:/kd/_proxq-'+metadata['build_id'].removeprefix('pro-x-')+'.prx'
            vsh = elf_data(tree / 'Vshctrl/vshctrl.prx')
            archived = header_bytes(tree / 'Installer/resident_archive.h')
            require(zlib.decompress(archived) == worker and installer.count(archived) == 1,
                    'Installer resident archive differs from the compiled worker')
            require(path.encode() in installer and path.encode() in vsh and
                    worker_sha.encode() in installer and bytes.fromhex(worker_sha) in vsh,
                    'Installer and loader must pin the same internal path and digest')
            require(worker.count(ca_bundle + b'\0') == 1, 'Compiled resident trust differs from pinned CA')
            require(b'PROX-RESIDENT-FLASH0-V1' in worker and b'Resident query worker source: flash0' in worker and
                    b'PSP/GAME/PROXUPDATE' not in worker and (dedicated_ota or b'flash0:/' not in worker) and
                    b'sceKernelLoadExec' not in updater_elf and b'prox_stage_archive' not in updater_elf,
                    'Resident worker is not isolated to authenticated query and removable reports')
            if dedicated_ota:
                require(all(symbol in updater_elf for symbol in (b'prox_native_background_launch_dedicated',
                        b'prox_dedicated_writer_check', b'prox_completion_read_store',
                        b'prox_dedicated_cleanup_committed', b'Dedicated boot cleanup:')) and
                        b'prox_dedicated_writer_prepare' not in updater_elf and
                        b'DEDICATED PREVIEW:' not in updater_elf and b'PROX-DEDICATED-TEST.PBP' not in updater_elf,
                        'Writable resident flow must use the bound local engine and exclude fixtures')
                cleanup = (tree / 'Vshctrl/native_dedicated_cleanup.o').read_bytes()
                require(b'prox_native_cleanup_flash' in cleanup and b'sceIoRemove' in cleanup and
                        b'sceIoWrite' not in cleanup and b'prox_native_mount_end' in cleanup,
                        'Dedicated post-boot cleanup must only retire verified sidecars and restore flash access')
            require(b'PSP/GAME/PROXUPDATE/EBOOT.PBP' not in vsh and b'PROXUPDATE/.native-' not in vsh and
                    b'resident flash0 image verified and loaded' in vsh,
                    'Resident VSH must have no card-based updater fallback')
            embedded['Updater/prox_updater.prx'] = worker_sha
        else:
            client = pbp_info((output / 'PSP/GAME/PROXUPDATE/EBOOT.PBP').read_bytes(), allow_plain=True)
            require(client['title'] == 'PRO-X v'+VERSION+' System Update', 'Wrong updater product title')
            require((output / 'PSP/GAME/PROXUPDATE/cacert.pem').read_bytes() == ca_bundle, 'Packaged CA bundle changed')
            info['PROXUPDATE'] = client
        if metadata.get('native_background'):
            client_elf=(tree/'Updater/prox_updater.elf').read_bytes()
            if not dedicated_ota:
                generated=(tree/'Updater/native_signed_templates.h').read_bytes()
                signer_source=(ROOT/'firmware/contrib/PrxEncrypter/main.c').read_bytes()
                require(generated==render_signed_templates(signer_source),'Signed decoder templates differ from retained source')
                require(b'PROX-NATIVE-DATA-ONLY-V1' in client_elf and sha(signer_source).encode() in client_elf,
                        'Data-only decoder/template binding missing')
            require(b'kuKernelLoadModuleWithApitype2' not in client_elf and
                    b'sceUtilsBufferCopyWithRange' not in client_elf,
                    'Target device loader or replacement PSP crypto service linked')
            engine=(tree/'tools/nativeinstaller/native_installer.prx').read_bytes()
            require(header_bytes(tree/'Updater/native_engine.h')==engine,'Background writer header differs')
            require((tree/'Updater/prox_updater.elf').read_bytes().count(engine)==1,'Matching background writer absent/duplicated')
            require(sha(engine).encode() in (tree/'Updater/prox_updater.elf').read_bytes(),'Background writer hash binding absent')
            require(b'PROX-NATIVE-INSTALL-WRITER-V1' in engine,'Background writer identity absent')
            for symbol in (b'proxNativeInstallService',b'proxNativeCompletionControl'):
                require(hashlib.sha1(symbol).digest()[:4] in engine,'Background writer private import absent')
            engine_elf=(tree/'tools/nativeinstaller/native_installer.elf').read_bytes()
            require(elf_u32_symbol(engine_elf,'sce_newlib_attribute')==0xc0004000,
                    'Background writer must create its main thread in VSH API class with VFPU')
            require(b'kuKernelGetUserLevel' in engine_elf and b'VSH API expected level=4' in engine_elf,
                    'Background writer effective VSH-level guard absent')
            for forbidden in (b'sceIoAssign\0',b'sceIoUnassign\0',b'kuKernelCall\0',b'kuKernelPokew\0'):
                require(forbidden not in engine_elf,'Background engine has an unsafe live-mount capability')
            require(b'Background access BEGIN' in engine_elf and b'Background access END' in engine_elf and
                    b'resident access revision mismatch' in engine_elf,'Missing access lease or resident capability guard')
            commit_markers = ((b'TRANSACTION stage=dedicated-card-record', b'prox_dedicated_writer_prepare',
                               b'--prox-native-install-v2') if dedicated_ota else
                              (b'TRANSACTION stage=application-commit', b'STORAGE_MOVE operation='))
            require(all(marker in engine_elf for marker in (b'TRANSACTION stage=firmware-prepare',b'TRANSACTION stage=firmware-replace',
                    b'TRANSACTION stage=firmware-sync',b'FLASH_FAIL operation=',
                    b'FLASH_ROLLBACK result=',b'FLASH_BACKUP verified',
                    b'TRANSACTION stage=backup-flush',b'original flashfat rename unsupported') + commit_markers),'Missing checked-copy backup/flush or exact transaction failure evidence')
            provider=(tree/'Vshctrl/native_query_provider.o').read_bytes()
            require(max(elf_function_frames(provider,'proxNativeInstallService'))<=128 and
                    b'sceKernelExtendKernelStack' in provider,'Access RPC regressed to a large syscall stack')
            vsh=(tree/'Vshctrl/vshctrl.elf').read_bytes()
            require(all(symbol in vsh for symbol in (b'prox_native_mount_begin',b'prox_native_mount_end',b'prox_mount_lease_end')),
                    'Matching mounted-access kernel implementation missing')
            if dedicated_ota:
                require(all(symbol in vsh for symbol in (b'prox_ni_arm_dedicated', b'prox_native_completion_armed',
                        b'prox_native_flash_preflight_dedicated')), 'Dedicated writer ownership, notice or capacity gate missing')
            for forbidden in (b'pspDebugScreen',b'sceDisplaySetFrameBuf',b'sceKernelLoadExec',b'sceGuInit'):
                require(forbidden not in engine_elf,'Background engine UI/LoadExec not allowed')
            embedded['tools/nativeinstaller/native_installer.prx']=sha(engine)
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
