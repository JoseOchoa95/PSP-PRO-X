/* Post-boot cleanup may remove verified PRO-X sidecars only. The temporary
 * writable mount stays inside this worker-owned, fixed-inventory operation. */
#ifdef PROX_NATIVE_CLEANUP_TEST
#include "resident_loader_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspsysmem_kernel.h>
#endif
#include <mbedtls/sha256.h>
#include <string.h>
#include "native_dedicated_cleanup.h"
#include "native_mount_access.h"
#include "prox_native_flash_paths.h"
#include "prox_resident_image.h"
#include "prox_resident_slot.h"

_Static_assert(PROX_NATIVE_FLASH_FILES == PROX_NI_FLASH_PARTS, "Cleanup flash inventory mismatch");
static const int missing = (int)0x80010002;

static int valid_part(const ProxNativePart *part, unsigned int limit)
{
    return part->size && part->size <= limit && !part->sha256[64] &&
           strspn(part->sha256, "0123456789abcdef") == 64;
}

static int matches(const char *path, const ProxNativePart *part, const SceIoStat *info)
{
    static const char digits[] = "0123456789abcdef";
    unsigned char block[4096], digest[32];
    mbedtls_sha256_context ctx;
    if (!FIO_S_ISREG(info->st_mode) || info->st_size != part->size)
        return -1;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return -1;
    int result = -1;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts(&ctx, 0))
        goto done;
    unsigned int remaining = part->size;
    while (remaining) {
        unsigned int count = remaining < sizeof(block) ? remaining : sizeof(block);
        if (sceIoRead(fd, block, count) != (int)count || mbedtls_sha256_update(&ctx, block, count))
            goto done;
        remaining -= count;
    }
    if (sceIoRead(fd, block, 1) != 0 || mbedtls_sha256_finish(&ctx, digest))
        goto done;
    for (unsigned int i = 0; i < sizeof(digest); ++i) {
        if (digits[digest[i] >> 4] != part->sha256[2 * i] || digits[digest[i] & 15u] != part->sha256[2 * i + 1])
            goto done;
    }
    result = 0;
done:
    mbedtls_sha256_free(&ctx);
    return sceIoClose(fd) < 0 ? -1 : result;
}

static int installed_matches(const char *path, const ProxNativePart *part)
{
    SceIoStat info;
    return sceIoGetstat(path, &info) < 0 || matches(path, part, &info) ? -1 : 0;
}

static int sidecar(const char *base, const char *suffix, const ProxNativePart *part, int remove_now)
{
    char path[96];
    SceIoStat info;
    size_t base_length = strlen(base);
    size_t suffix_length = strlen(suffix);
    if (base_length + suffix_length >= sizeof(path))
        return -1;
    memcpy(path, base, base_length);
    memcpy(path + base_length, suffix, suffix_length + 1);
    int status = sceIoGetstat(path, &info);
    if (status == missing)
        return 0;
    if (status < 0 || matches(path, part, &info) || (remove_now && sceIoRemove(path) < 0))
        return -1;
    return 1;
}

static int sidecars(const ProxNativeCleanupMessage *job, const char *worker, int remove_now)
{
    int found = 0;
    for (unsigned int i = 0; i < PROX_NI_FLASH_PARTS; ++i) {
        unsigned int bit = 1u << i;
        if (job->prepared & bit) {
            int result = sidecar(prox_native_flash_paths[i], ".prox-new", &job->installed[i], remove_now);
            if (result < 0)
                return -1;
            found += result;
        }
        if (job->backups & bit) {
            int result = sidecar(prox_native_flash_paths[i], ".prox-old", &job->before[i], remove_now);
            if (result < 0)
                return -1;
            found += result;
        }
    }
    if (job->worker_prepared) {
        int result = sidecar(worker, ".prox-new", &job->worker, remove_now);
        if (result < 0)
            return -1;
        found += result;
    }
    return found;
}

int prox_native_cleanup_flash(const ProxNativeCleanupMessage *job)
{
    const unsigned int mask = (1u << PROX_NI_FLASH_PARTS) - 1u;
    if (!job || sceKernelDevkitVersion() != 0x06060110u || sceKernelGetModel() != 1 || job->model != 1 ||
        !memchr(job->build_id, 0, sizeof(job->build_id)) || strcmp(job->build_id, PROX_RESIDENT_BUILD_ID) ||
        (job->prepared & ~mask) || (job->backups & ~job->prepared) || job->worker_prepared > 1 ||
        !valid_part(&job->worker, 1024u * 1024u) || job->worker.size != PROX_RESIDENT_IMAGE_SIZE ||
        strcmp(job->worker.sha256, PROX_RESIDENT_IMAGE_DIGEST))
        return -1;
    for (unsigned int i = 0; i < PROX_NI_FLASH_PARTS; ++i) {
        if (!valid_part(&job->installed[i], 1024u * 1024u) ||
            ((job->backups & (1u << i)) && !valid_part(&job->before[i], 16u * 1024u * 1024u)) ||
            installed_matches(prox_native_flash_paths[i], &job->installed[i]))
            return -1;
    }
    if (installed_matches(PROX_RESIDENT_IMAGE_PATH, &job->worker))
        return -1;
    int found = sidecars(job, PROX_RESIDENT_IMAGE_PATH, 0);
    if (found <= 0)
        return found;

    ProxMountReport access = {0};
    int result = prox_native_mount_begin(&access);
    if (!result && access.active)
        result = sidecars(job, PROX_RESIDENT_IMAGE_PATH, 1) < 0 ? -1 : 0;
    else
        result = -1;
    if (!access.active)
        return result;
    if (prox_native_mount_end(&access) || !access.restored || access.active)
        return -2;
    return result;
}
