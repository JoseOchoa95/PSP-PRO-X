/* Load one pinned user-mode image from flash0 without a card. GPL-3.0-or-later. */
#ifdef PROX_RESIDENT_LOADER_TEST
#include "resident_loader_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include "systemctrl.h"
#endif
#include <stdint.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include "native_query_provider.h"
#include "prox_resident_image.h"

void mbedtls_platform_zeroize(void *buffer, size_t size)
{
    volatile unsigned char *byte = buffer;
    while (size--)
        *byte++ = 0;
}

typedef int (*Alloc)(int, const char *, int, unsigned int, void *);
typedef void *(*Head)(int);
typedef int (*Free)(int);
/* Exact size-first ModuleMgr ABI, also used by the previous card cache. */
typedef int (*LoadBuffer661)(SceSize, void *, int, SceKernelLMOption *);

int prox_native_resident_load(void)
{
    static const unsigned char expected[] = PROX_RESIDENT_IMAGE_BYTES;
    unsigned char digest[32], extra, *buffer;
    unsigned int copied = 0;
    int allocation, fd = -1, module = -1;
    SceIoStat stat;
    SceKernelLMOption option;
    Alloc alloc;
    Head head;
    Free release;
    LoadBuffer661 load;

    if (sceKernelDevkitVersion() != 0x06060110u ||
        sceIoGetstat(PROX_RESIDENT_IMAGE_PATH, &stat) < 0 ||
        !FIO_S_ISREG(stat.st_mode) || stat.st_size != PROX_RESIDENT_IMAGE_SIZE)
        return -1;
    alloc = (Alloc)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0x237dbd4f);
    head = (Head)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0x9d9a5ba1);
    release = (Free)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0xb6d61d02);
    load = (LoadBuffer661)sctrlHENFindFunction("sceModuleManager", "ModuleMgrForKernel", 0x4e62c48a);
    if (!alloc || !head || !release || !load)
        return -1;
    allocation = alloc(2, "PRO-X internal query image", PSP_SMEM_High, PROX_RESIDENT_IMAGE_SIZE + 63, NULL);
    if (allocation < 0)
        return allocation;
    buffer = head(allocation);
    if ((uintptr_t)buffer < 0x08800000u || (uintptr_t)buffer > 0x0c000000u - PROX_RESIDENT_IMAGE_SIZE - 63)
        goto done;
    buffer = (unsigned char *)(((uintptr_t)buffer + 63u) & ~(uintptr_t)63u);
    fd = sceIoOpen(PROX_RESIDENT_IMAGE_PATH, PSP_O_RDONLY, 0);
    if (fd < 0)
        goto done;
    while (copied < PROX_RESIDENT_IMAGE_SIZE) {
        unsigned int amount = PROX_RESIDENT_IMAGE_SIZE - copied;
        if (amount > 16384)
            amount = 16384;
        if (sceIoRead(fd, buffer + copied, amount) != (int)amount)
            goto done;
        copied += amount;
    }
    if (sceIoRead(fd, &extra, 1) != 0)
        goto done;
    int closed = sceIoClose(fd);
    fd = -1;
    if (closed < 0 || mbedtls_sha256(buffer, PROX_RESIDENT_IMAGE_SIZE, digest, 0) ||
        memcmp(digest, expected, sizeof(digest)))
        goto done;

    memset(&option, 0, sizeof(option));
    option.size = sizeof(option);
    option.access = 1;
    option.position = 1;
    sceKernelDcacheWritebackInvalidateAll();
    /* ModuleMgr copies its input. The bytes passed here are the bytes hashed. */
    module = load(PROX_RESIDENT_IMAGE_SIZE, buffer, 0, &option);
done:
    if (fd >= 0)
        sceIoClose(fd);
    release(allocation);
    return module;
}
