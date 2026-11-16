"""Validate the real native variant before making normal distribution assets.

Packaging does not authorize a tag, release, PR or Git operation.
"""
from pathlib import Path
from verify import require


def require_release_variant(build):
    require(build.get('mode')=='pro-x' and build.get('native_ota') is True,
            'Normal distribution requires the real native OTA build')
    require(not any(build.get(name) for name in
                    ('native_query_only','native_offer_preview','native_progress_preview','test_only',
                     'simulated_positive_offer','simulated_incomplete_transfer')),
            'Query-only, preview and temporary-version builds cannot produce stable OTA assets')


def verify_native_release(root,build):
    require_release_variant(build)
    output=Path(root)/build['output']/'build-tree'
    vsh=(output/'Vshctrl/vshctrl.elf').read_bytes()
    client=(output/'Updater/prox_updater.elf').read_bytes()
    for marker in (b'native REAL compatible offer; awaiting consent',b'native query RAM: size-first load',
                   b'ProxNativeQueryForUser',b'ProxNativePreviewForUser'):
        require(marker in vsh,'Missing native release implementation')
    require(b'sceKernelLoadModuleBuffer\0' not in vsh,'Unsafe legacy buffer-loader import')
    require(b'PREVIEW simulated offer' not in vsh and b'native probe simulated' not in vsh,
            'Simulated native frontend cannot be distributed as stable')
    require(b'PROX-NATIVE-CARDLESS-V1' in client and b'PROX-NATIVE-QUERY-V3' in client and
            b'Verified native handoff to target installer' in client,'Missing real native worker')
    ca=(Path(root)/'firmware/Updater/cacert.pem').read_bytes()
    require(client.count(ca+b'\0')==1 and
            (Path(root)/build['output']/'PSP/GAME/PROXUPDATE/cacert.pem').read_bytes()==ca,
            'Resident and manual-package trust bytes differ')
