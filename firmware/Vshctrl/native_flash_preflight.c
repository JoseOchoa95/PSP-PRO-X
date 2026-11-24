/* Shared OTA space check for the verified writer and read-only preview. */
#ifdef PROX_NATIVE_FLASH_PREFLIGHT_TEST
#include "resident_loader_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspsdk.h>
#include <pspthreadman_kernel.h>
#endif
#include "prox_flash_capacity.h"
#include "prox_native_flash_paths.h"
#include "native_flash_preflight.h"
#include "native_query_provider.h"

static uint32_t allocated(uint32_t size, uint32_t cluster)
{
    return (size + cluster - 1) & ~(cluster - 1);
}

static int budget(const ProxFlashCapacity *capacity, const ProxNativePart *parts, unsigned int *required)
{
    uint32_t cluster = capacity->cluster_bytes;
    if (cluster < PROX_FLASH_SECTOR_BYTES || cluster > 128 * PROX_FLASH_SECTOR_BYTES ||
        (cluster & (cluster - 1)) || capacity->free_bytes > capacity->total_bytes ||
        capacity->total_bytes > PROX_FLASH_MAX_VOLUME_BYTES)
        return -1;
    /* Each path plus its .prox-new/.prox-old sidecars can use up to four
     * FAT directory entries. Reserve extra room because they span two dirs. */
    *required = allocated(PROX_NATIVE_FLASH_FILES * 3 * 4 * 32, cluster) + cluster;
    for (unsigned int i = 0; i < PROX_NATIVE_FLASH_FILES; ++i) {
        SceIoStat st;
        unsigned int size = parts[i].size;
        int result = sceIoGetstat(prox_native_flash_paths[i], &st);
        if (!size || size > 1024u * 1024u)
            return -1;
        if (result >= 0) {
            if (!FIO_S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 16u * 1024u * 1024u)
                return -1;
            *required += allocated((uint32_t)st.st_size, cluster);
        } else if (result != (int)0x80010002) {
            return -1;
        }
        /* Count a staged replacement and a complete destination. Keep all
         * original allocations charged and assume no reclaim from overwrite. */
        *required += 2 * allocated(size, cluster);
    }
    return 0;
}

static int preflight(const ProxNativePart *parts, unsigned int worker_size)
{
    ProxFlashCapacity capacity;
    unsigned int required;
    int result = prox_flash0_capacity(&capacity);
    if (result) {
        prox_native_query_event("OTA flash0 measurement failed", result);
        return result;
    }
    if (!parts || budget(&capacity, parts, &required)) {
        prox_native_query_event("OTA flash0 budget could not be established", -1);
        return -1;
    }
    if (worker_size) {
        unsigned int cluster = capacity.cluster_bytes;
        /* Keep the previous resident slot. Reserve the complete new worker,
         * its checked sidecar and additional directory growth in kd. */
        required += 2 * allocated(worker_size, cluster) + cluster;
    }
    prox_native_query_event("OTA flash0 free bytes", (int)capacity.free_bytes);
    prox_native_query_event("OTA flash0 required bytes", (int)required);
    prox_native_query_event("OTA flash0 cluster bytes", (int)capacity.cluster_bytes);
    result = capacity.free_bytes < required ? -4 : 0;
    prox_native_query_event("OTA flash0 preflight result", result);
    return result;
}

int prox_native_flash_preflight(const ProxNativePart *parts)
{
    return preflight(parts, 0);
}

int prox_native_flash_preflight_dedicated(const ProxNativePart *parts, const ProxNativePart *worker)
{
    if (!worker || !worker->size || worker->size > 1024u * 1024u)
        return -1;
    return preflight(parts, worker->size);
}

#ifdef PROX_NATIVE_PROGRESS_PREVIEW
static void preview_measure(void *argument)
{
    /* Fixed target sizes from the v0.4.0 resident diagnostic, in writer order.
     * Current originals and free space are measured on this PSP. */
    static const ProxNativePart parts[] = {
        {.size = 35643}, {.size = 24028}, {.size = 6849}, {.size = 10950},
        {.size = 15022}, {.size = 11492}, {.size = 4294}, {.size = 12501},
        {.size = 87058}, {.size = 116274}
    };
    _Static_assert(sizeof(parts) / sizeof(parts[0]) == PROX_NATIVE_FLASH_FILES, "Flash inventory mismatch");
    *(int *)argument = prox_native_flash_preflight(parts);
}

int prox_native_flash_capacity_preview(void)
{
    int result = -1;
    int k1 = pspSdkSetK1(0);
    int status = sceKernelExtendKernelStack(0x4000, preview_measure, &result);
    pspSdkSetK1(k1);
    return status < 0 ? status : result;
}
#endif
