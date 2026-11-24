/* Measure and budget flash0 before the manual resident installation. GPL-3.0-or-later. */
#ifdef PROX_CAPACITY_TEST
#include "resident_loader_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>
#include "kubridge.h"
#endif
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>
#include "stage.h"
#include "prox_flash_capacity.h"
#include "prox_resident_image.h"
#include "flash_capacity_result.h"
#include "flash_capacity_prx.h"

static int measure_flash(const char *installer_path)
{
    char path[256], hex[65];
    unsigned char digest[32];
    const char *slash = installer_path ? strrchr(installer_path, '/') : NULL;
    if (!slash || strncmp(installer_path, "ms0:/", 5) || strcmp(slash, "/installer.prx"))
        return -1;
    size_t prefix = (size_t)(slash - installer_path) + 1;
    if (prefix + sizeof(PROX_CAPACITY_TEMP_FILE) > sizeof(path))
        return -1;
    memcpy(path, installer_path, prefix);
    memcpy(path + prefix, PROX_CAPACITY_TEMP_FILE, sizeof(PROX_CAPACITY_TEMP_FILE));

    /* Own the temporary card file from exclusive creation through deletion.
     * It is removed after the module loader has copied it into memory. */
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_EXCL, 0600);
    if (fd < 0)
        return fd;
    int written = sceIoWrite(fd, flash_capacity_prx, size_flash_capacity_prx);
    int closed = sceIoClose(fd);
    int module = -1;
    if (written == (int)size_flash_capacity_prx && closed >= 0 &&
        !mbedtls_sha256(flash_capacity_prx, size_flash_capacity_prx, digest, 0)) {
        for (unsigned int i = 0; i < sizeof(digest); ++i)
            snprintf(hex + 2 * i, 3, "%02x", digest[i]);
        if (!prox_file_check(path, size_flash_capacity_prx, hex, 0))
            module = kuKernelLoadModule(path, 0, NULL);
    }
    int removed = sceIoRemove(path);
    if (removed < 0) {
        if (module >= 0)
            sceKernelUnloadModule(module);
        pspDebugScreenPrintf("Could not remove %s (0x%08x).\n", path, (unsigned int)removed);
        return removed;
    }
    if (module < 0)
        return module;

    /* The tagged nonzero result tells Module Manager to unload the one-shot
     * module. A failed start may leave its loaded image to be unloaded here. */
    int status = -1;
    SceKernelSMOption option = { .size = sizeof(option), .stacksize = 16 * 1024 };
    int started = sceKernelStartModule(module, 0, NULL, &status, &option);
    if (started == module) {
        if (sceKernelStopModule(module, 0, NULL, NULL, NULL) != 0 ||
            sceKernelUnloadModule(module) < 0)
            return -1;
    } else if (started < 0) {
        sceKernelUnloadModule(module);
        return started;
    }
    return ((unsigned int)status & PROX_CAPACITY_RESULT_MASK) == PROX_CAPACITY_RESULT_TAG ? status : -1;
}

static uint32_t allocated_bytes(uint32_t bytes, uint32_t cluster_bytes)
{
    return (bytes + cluster_bytes - 1) & ~(cluster_bytes - 1);
}

int prox_resident_capacity_ready(const char *installer_path, const uint32_t *module_sizes,
                                 unsigned int module_count)
{
    int result = measure_flash(installer_path);
    if (result < 0) {
        pspDebugScreenPrintf("Could not measure flash0 free space (0x%08x).\n", (unsigned int)result);
        return -1;
    }
    uint32_t cluster_bytes = PROX_FLASH_SECTOR_BYTES << ((result >> 16) & 7);
    uint64_t free_bytes = (uint64_t)(result & 0xffff) * cluster_bytes;
    if (free_bytes > PROX_FLASH_MAX_VOLUME_BYTES)
        return -1;
    /* Budget full target copies without credit for existing files. Each of
     * our filenames fits in at most three 32-byte FAT directory entries.
     * Round the combined entries up and reserve another cluster because
     * the entries are split across kd and vsh/module. */
    uint32_t directory_bytes = allocated_bytes((module_count + 1) * 3 * 32, cluster_bytes) + cluster_bytes;
    uint64_t required = directory_bytes + allocated_bytes(PROX_RESIDENT_IMAGE_SIZE, cluster_bytes);
    for (unsigned int i = 0; i < module_count; ++i) {
        if (!module_sizes[i] || module_sizes[i] > PROX_FLASH_MAX_VOLUME_BYTES)
            return -1;
        required += allocated_bytes(module_sizes[i], cluster_bytes);
    }
    pspDebugScreenPrintf("flash0 free: %u bytes, required: %u (cluster: %u).\n",
                         (unsigned int)free_bytes, (unsigned int)required, (unsigned int)cluster_bytes);
    if (free_bytes < required) {
        pspDebugScreenPrintf("Not enough flash0 space. No firmware write started.\n");
        return -1;
    }
    return 0;
}
