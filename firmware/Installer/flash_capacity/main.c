/* One-shot read-only flash0 measurement. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <string.h>
#include "prox_flash_capacity.h"
#include "../flash_capacity_result.h"

PSP_MODULE_INFO("ProxInstallerCapacity", PSP_MODULE_KERNEL, 1, 0);

/* syslib declares the module-start stack used by the FAT parser. */
const unsigned int module_start_thread_parameter[] = {3, 32, 16 * 1024, 0};

int module_start(SceSize args, void *argp)
{
    ProxFlashCapacity capacity;
    uint32_t request;

    if (args != sizeof(request) || !argp)
        return PROX_FLASH_CAPACITY_GEOMETRY;
    memcpy(&request, argp, sizeof(request));
    if (request != PROX_CAPACITY_REQUEST_TAG)
        return PROX_FLASH_CAPACITY_GEOMETRY;
    int result = prox_flash0_capacity(&capacity);
    if (result)
        return result;
    unsigned int shift = 0;
    while ((PROX_FLASH_SECTOR_BYTES << shift) < capacity.cluster_bytes)
        ++shift;
    return PROX_CAPACITY_RESULT_TAG | (shift << 16) | (capacity.free_bytes / capacity.cluster_bytes);
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}
