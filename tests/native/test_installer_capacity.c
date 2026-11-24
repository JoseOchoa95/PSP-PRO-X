#include "resident_loader_shim.h"
#include "flash_capacity_result.h"
#include "flash_capacity_prx.h"
#include "prox_resident_image.h"
#include <assert.h>
#include <mbedtls/sha256.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int prox_resident_capacity_ready(const char *, const uint32_t *, unsigned int);

static const char installer[] = "ms0:/PSP/GAME/PROUPDATE/installer.prx";
static const char helper[] = "ms0:/PSP/GAME/PROUPDATE/" PROX_CAPACITY_TEMP_FILE;
static int exists, opened, fail_close, fail_validate, fail_load, fail_remove, fail_start;
static int starts, unloads, stops, writes, reported;
static unsigned int cluster_shift = 5;
static unsigned int free_clusters = 7;

int sceIoOpen(const char *path, int flags, int mode)
{
    assert(!strcmp(path, helper) && flags == (PSP_O_WRONLY | PSP_O_CREAT | PSP_O_EXCL) && mode == 0600);
    if (exists)
        return -17;
    exists = opened = 1;
    return 31;
}

int sceIoWrite(int fd, const void *bytes, unsigned int size)
{
    assert(fd == 31 && opened && size == size_flash_capacity_prx);
    assert(!memcmp(bytes, flash_capacity_prx, size));
    ++writes;
    return size;
}

int sceIoClose(int fd)
{
    assert(fd == 31 && opened);
    opened = 0;
    return fail_close ? -1 : 0;
}

int sceIoRemove(const char *path)
{
    assert(!strcmp(path, helper) && exists && !opened);
    if (fail_remove)
        return -1;
    exists = 0;
    return 0;
}

int prox_file_check(const char *path, unsigned int size, const char *hex, int pbp)
{
    unsigned char digest[32];
    char expected[65];
    assert(!strcmp(path, helper) && exists && !opened && !pbp && size == size_flash_capacity_prx);
    assert(!mbedtls_sha256(flash_capacity_prx, size, digest, 0));
    for (unsigned int i = 0; i < sizeof(digest); ++i)
        snprintf(expected + 2 * i, 3, "%02x", digest[i]);
    assert(!strcmp(hex, expected));
    return fail_validate ? -1 : 0;
}

int kuKernelLoadModule(const char *path, int flags, SceKernelLMOption *option)
{
    assert(!strcmp(path, helper) && exists && !opened && !flags && !option);
    return fail_load ? -1 : 42;
}

int sceKernelStartModule(int module, SceSize args, void *argp, int *status, SceKernelSMOption *option)
{
    assert(module == 42 && !exists && !opened && !args && !argp &&
           option->size == sizeof(*option) && option->stacksize >= 8192);
    ++starts;
    if (fail_start) {
        *status = 0;
        return module;
    }
    *status = PROX_CAPACITY_RESULT_TAG | (cluster_shift << 16) | free_clusters;
    return *status;
}

int sceKernelStopModule(int module, SceSize args, void *argp, int *status, SceKernelSMOption *option)
{
    assert(module == 42 && !args && !argp && !status && !option);
    ++stops;
    return 0;
}

int sceKernelUnloadModule(int module)
{
    assert(module == 42);
    ++unloads;
    return 0;
}

void pspDebugScreenPrintf(const char *format, ...)
{
    if (strstr(format, "flash0 free:"))
        ++reported;
}

int main(void)
{
    const uint32_t sizes[] = {1, 16385};
    assert(!prox_resident_capacity_ready(installer, sizes, 2) && starts == 1 && reported == 1);
    /* Worker (1 cluster), both modules (1 and 2), and 2 directory clusters. */
    free_clusters = 6;
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    free_clusters = 5;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);

    int before = starts;
    assert(prox_resident_capacity_ready("flash0:/installer.prx", sizes, 2) < 0);
    exists = 1;
    int previous_writes = writes;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && exists && writes == previous_writes);
    exists = 0;
    fail_close = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    fail_close = 0;
    fail_validate = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    fail_validate = 0;
    fail_load = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    fail_load = 0;
    fail_remove = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && exists && unloads == 1);
    fail_remove = exists = 0;
    assert(starts == before);
    fail_start = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && stops == 1 && unloads == 2 && !exists);
    fail_start = 0;
    cluster_shift = 7;
    free_clusters = 65535;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    const uint32_t small_files[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    cluster_shift = 0;
    free_clusters = 15; /* Eleven file clusters and four directory clusters. */
    assert(!prox_resident_capacity_ready(installer, small_files, 10));
    free_clusters = 14;
    assert(prox_resident_capacity_ready(installer, small_files, 10) < 0 && !exists);
    puts("PASS: exact cluster budget, exclusive card staging, checked cleanup, and invalid measurement rejection");
}
