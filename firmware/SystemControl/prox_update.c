/* PRO-X private updater services, GPL-3.0-or-later.
 * Additive library; legacy SystemCtrl exports, HEN values and ABI unchanged. */
#ifdef PROX_BRIDGE_NATIVE
#include "update_bridge_shim.h"
#else
#include <pspkernel.h>
#include <pspsdk.h>
#include <pspsysmem_kernel.h>
#include "pspcrypt.h"
#include "main.h"
#endif
#include <string.h>
#include "prox_product.h"

int proxUpdateGetState(int selector)
{
    switch (selector) {
    case 0: return 1; /* private protocol version */
    case 1: return PROX_PRODUCT_MAJOR;
    case 2: return PROX_PRODUCT_MINOR;
    case 3: return PROX_PRODUCT_PATCH;
    /* SystemControl initializes these with the firmware-specific CrossFW
     * imports before exposing the bridge. The generic SDK imports can remain
     * unlinked on 6.61 (0x8002013a). Do not query them from this syscall. */
    case 4: return (int)psp_fw_version;
    case 5: return (int)psp_model;
    default: return -1;
    }
}

int proxUpdateRandom(void *output, unsigned int length)
{
    unsigned char block[20] __attribute__((aligned(64)));
    unsigned char *destination = output;
    unsigned int position = 0, n, address = (unsigned int)output;
    int k1, rc = 0;
    /* Only bounded user RAM, checked before dropping K1. Never return bytes
     * if KIRK reports a failure; never fall back to time, rand() or MT19937. */
    if (!length || length > 256 || address < 0x08800000 ||
        address > 0x0c000000 - length) return -1;
    k1 = pspSdkSetK1(0);
    while (position < length) {
        memset(block, 0, sizeof(block));
        rc = sceUtilsBufferCopyWithRange(block, sizeof(block), NULL, 0, 14);
        if (rc != 0) { rc = -1; break; }
        n = length - position;
        if (n > sizeof(block)) n = sizeof(block);
        memcpy(destination + position, block, n);
        position += n;
    }
    if (rc) memset(destination, 0, length);
    memset(block, 0, sizeof(block));
    pspSdkSetK1(k1);
    return rc;
}
