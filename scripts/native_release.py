"""Validate the real native variant before making normal distribution assets.

Packaging does not authorize a tag, release, PR or Git operation.
"""
from pathlib import Path
import re
from verify import require


def require_release_variant(build):
    require(build.get('mode')=='pro-x' and build.get('native_ota') is True,
            'Normal distribution requires the real native OTA build')
    require(not any(build.get(name) for name in
                    ('native_query_only','native_offer_preview','native_progress_preview','native_install_preview','test_only',
                     'resident_dedicated_preview','simulated_positive_offer','simulated_incomplete_transfer')),
            'Query-only, preview and temporary-version builds cannot produce stable OTA assets')
    version=build.get('product_version')
    if version is not None:
        require(re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)',version),
                'Normal distribution requires a numeric stable product version')
    resident_version=version is not None and tuple(map(int,version.split('.')))>=(0,4,0)
    if resident_version or build.get('resident_query') or build.get('resident_dedicated_ota'):
        require(all(build.get(name) is True for name in
                    ('resident_query','resident_dedicated_ota','native_background')),
                'Resident distribution requires the writable dedicated updater')


def verify_native_release(root,build):
    require_release_variant(build)
    compiled=Path(root)/build['output']
    output=compiled/'build-tree'
    resident=build.get('resident_dedicated_ota',False)
    if not resident:
        require((compiled/'PSP/GAME/PROUPDATE/EBOOT.PBP').stat().st_size<=2*1024*1024,
                'Installer exceeds the bounded background target reader')
    vsh=(output/'Vshctrl/vshctrl.elf').read_bytes()
    client=(output/'Updater/prox_updater.elf').read_bytes()
    loader=b'resident flash0 image verified and loaded' if resident else b'native query RAM: size-first load'
    for marker in (b'native REAL compatible offer; awaiting consent',loader,
                   b'ProxNativeQueryForUser',b'ProxNativePreviewForUser'):
        require(marker in vsh,'Missing native release implementation')
    require(b'sceKernelLoadModuleBuffer\0' not in vsh,'Unsafe legacy buffer-loader import')
    require(b'PREVIEW simulated offer' not in vsh and b'native probe simulated' not in vsh,
            'Simulated native frontend cannot be distributed as stable')
    worker=b'PROX-RESIDENT-FLASH0-V1' if resident else b'PROX-NATIVE-CARDLESS-V1'
    require(worker in client and b'PROX-NATIVE-QUERY-V3' in client,'Missing real native worker')
    if build.get('native_background'):
        require(b'PROX-NATIVE-BACKGROUND-INSTALL-V1' in client and b'proxNativeInstallService' in vsh,
                'Missing background worker/kernel service')
        decoder=b'prox_native_background_launch_dedicated' if resident else b'PROX-NATIVE-DATA-ONLY-V1'
        require(decoder in client and b'kuKernelLoadModuleWithApitype2' not in client,
                'Missing data-only decoder or obsolete target loader linked')
        engine=(output/'tools/nativeinstaller/native_installer.prx').read_bytes()
        require(client.count(engine)==1 and b'PROX-NATIVE-INSTALL-WRITER-V1' in engine,
                'Client does not embed its exact matched background writer')
        engine_elf=(output/'tools/nativeinstaller/native_installer.elf').read_bytes()
        for forbidden in (b'pspDebugScreen',b'sceDisplaySetFrameBuf',b'sceKernelLoadExec',b'sceGuInit'):
            require(forbidden not in engine_elf,'Background writer contains a renderer/LoadExec')
    else:
        require(b'Verified native handoff to target installer' in client,'Missing legacy real native handoff')
    ca=(Path(root)/'firmware/Updater/cacert.pem').read_bytes()
    require(client.count(ca+b'\0')==1,'Compiled worker trust bytes differ from the pinned CA')
    if resident:
        require(b'startup cleanup build' in client and not (compiled/'PSP/GAME/PROXUPDATE').exists(),
                'Resident distribution requires startup cleanup and a two-application manual kit')
    else:
        require((compiled/'PSP/GAME/PROXUPDATE/cacert.pem').read_bytes()==ca,
                'Manual-package trust bytes differ from the pinned CA')
