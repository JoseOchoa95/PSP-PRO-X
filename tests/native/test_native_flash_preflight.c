#include "resident_loader_shim.h"
#include "native_flash_preflight.h"
#include "prox_flash_capacity.h"
#include "prox_native_flash_paths.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned int free_clusters = 24;
static unsigned int checked_paths, reported_required;
static int read_error, stat_error, wrong_type;

int prox_flash0_capacity(ProxFlashCapacity *capacity)
{
    if (read_error)
        return PROX_FLASH_CAPACITY_READ;
    *capacity = (ProxFlashCapacity){.total_bytes = 100 * 16384, .free_bytes = free_clusters * 16384,
                                    .cluster_bytes = 16384, .fat_bits = 12};
    return 0;
}

int sceIoGetstat(const char *path, SceIoStat *stat)
{
    unsigned int i = checked_paths++;
    assert(i < PROX_NATIVE_FLASH_FILES && !strcmp(path, prox_native_flash_paths[i]));
    if (stat_error)
        return -13;
    if (i)
        return (int)0x80010002;
    stat->st_size = 16385;
    stat->st_mode = wrong_type ? 2 : 1;
    return 0;
}

void prox_native_query_event(const char *text, int value)
{
    if (!strcmp(text, "OTA flash0 required bytes"))
        reported_required = value;
}

int main(void)
{
    ProxNativePart parts[PROX_NI_PARTS] = {0};
    for (unsigned int i = 0; i < PROX_NATIVE_FLASH_FILES; ++i)
        parts[i].size = 1;
    /* 20 target clusters, two original backup clusters, two for directories. */
    assert(!prox_native_flash_preflight(parts) && checked_paths == PROX_NATIVE_FLASH_FILES);
    assert(reported_required == 24 * 16384);
    ProxNativePart worker = {.size = 16385};
    free_clusters = 28;
    checked_paths = 0;
    assert(prox_native_flash_preflight_dedicated(parts, &worker) == -4);
    assert(checked_paths == PROX_NATIVE_FLASH_FILES && reported_required == 29 * 16384);
    free_clusters = 29;
    checked_paths = 0;
    assert(!prox_native_flash_preflight_dedicated(parts, &worker));
    worker.size = 1024u * 1024u + 1;
    checked_paths = 0;
    assert(prox_native_flash_preflight_dedicated(parts, &worker) < 0 && !checked_paths);
    free_clusters = 23;
    checked_paths = 0;
    assert(prox_native_flash_preflight(parts) == -4 && checked_paths == PROX_NATIVE_FLASH_FILES);
    checked_paths = 0;
    read_error = 1;
    assert(prox_native_flash_preflight(parts) == PROX_FLASH_CAPACITY_READ && !checked_paths);
    read_error = 0;
    stat_error = 1;
    assert(prox_native_flash_preflight(parts) < 0 && checked_paths == 1);
    stat_error = 0;
    checked_paths = 0;
    wrong_type = 1;
    assert(prox_native_flash_preflight(parts) < 0 && checked_paths == 1);
    puts("PASS: OTA flash0 budget counts full staged targets, live original backups, directories and the dedicated worker");
}
