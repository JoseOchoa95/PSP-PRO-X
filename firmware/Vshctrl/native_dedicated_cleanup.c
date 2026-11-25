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

static int failed(ProxNativeCleanupDiagnostic *diagnostic, int check, int detail)
{
    diagnostic->check = check;
    diagnostic->detail = detail;
    return -1;
}

static int matches(const char *path, const ProxNativePart *part, const SceIoStat *info,
                   ProxNativeCleanupDiagnostic *diagnostic)
{
    static const char digits[] = "0123456789abcdef";
    unsigned char block[4096], digest[32];
    mbedtls_sha256_context ctx;
    if (!FIO_S_ISREG(info->st_mode) || info->st_size != part->size)
        return failed(diagnostic, PROX_CLEANUP_CHECK_SIZE, (int)info->st_size);
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return failed(diagnostic, PROX_CLEANUP_CHECK_OPEN, fd);
    int result = 0;
    mbedtls_sha256_init(&ctx);
    int status = mbedtls_sha256_starts(&ctx, 0);
    if (status) {
        result = failed(diagnostic, PROX_CLEANUP_CHECK_HASH, status);
        goto done;
    }
    unsigned int remaining = part->size;
    while (remaining) {
        unsigned int count = remaining < sizeof(block) ? remaining : sizeof(block);
        status = sceIoRead(fd, block, count);
        if (status != (int)count) {
            result = failed(diagnostic, PROX_CLEANUP_CHECK_READ, status);
            goto done;
        }
        status = mbedtls_sha256_update(&ctx, block, count);
        if (status) {
            result = failed(diagnostic, PROX_CLEANUP_CHECK_HASH, status);
            goto done;
        }
        remaining -= count;
    }
    status = sceIoRead(fd, block, 1);
    if (status) {
        result = failed(diagnostic, PROX_CLEANUP_CHECK_EOF, status);
        goto done;
    }
    status = mbedtls_sha256_finish(&ctx, digest);
    if (status) {
        result = failed(diagnostic, PROX_CLEANUP_CHECK_HASH, status);
        goto done;
    }
    for (unsigned int i = 0; i < sizeof(digest); ++i) {
        if (digits[digest[i] >> 4] != part->sha256[2 * i] || digits[digest[i] & 15u] != part->sha256[2 * i + 1]) {
            result = failed(diagnostic, PROX_CLEANUP_CHECK_HASH, (int)i);
            goto done;
        }
    }
done:
    mbedtls_sha256_free(&ctx);
    status = sceIoClose(fd);
    return !result && status < 0 ? failed(diagnostic, PROX_CLEANUP_CHECK_CLOSE, status) : result;
}

static int installed_matches(const char *path, const ProxNativePart *part, ProxNativeCleanupDiagnostic *diagnostic)
{
    SceIoStat info;
    int status = sceIoGetstat(path, &info);
    return status < 0 ? failed(diagnostic, PROX_CLEANUP_CHECK_STAT, status) : matches(path, part, &info, diagnostic);
}

static int sidecar(const char *base, const char *suffix, const ProxNativePart *part, int remove_now,
                   ProxNativeCleanupDiagnostic *diagnostic)
{
    char path[96];
    SceIoStat info;
    size_t base_length = strlen(base);
    size_t suffix_length = strlen(suffix);
    if (base_length + suffix_length >= sizeof(path))
        return failed(diagnostic, PROX_CLEANUP_CHECK_RECORD, (int)(base_length + suffix_length));
    memcpy(path, base, base_length);
    memcpy(path + base_length, suffix, suffix_length + 1);
    int status = sceIoGetstat(path, &info);
    if (status == missing)
        return 0;
    if (status < 0)
        return failed(diagnostic, PROX_CLEANUP_CHECK_STAT, status);
    if (matches(path, part, &info, diagnostic))
        return -1;
    if (remove_now) {
        status = sceIoRemove(path);
        if (status < 0)
            return failed(diagnostic, PROX_CLEANUP_CHECK_REMOVE, status);
    }
    return 1;
}

static int sidecars(ProxNativeCleanupMessage *job, const char *worker, int remove_now)
{
    ProxNativeCleanupDiagnostic *diagnostic = &job->diagnostic;
    int found = 0;
    for (unsigned int i = 0; i < PROX_NI_FLASH_PARTS; ++i) {
        unsigned int bit = 1u << i;
        if (job->prepared & bit) {
            *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_TEMP_PART, .part=(int)i};
            int result = sidecar(prox_native_flash_paths[i], ".prox-new", &job->installed[i], remove_now, diagnostic);
            if (result < 0)
                return -1;
            found += result;
        }
        if (job->backups & bit) {
            *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_BACKUP, .part=(int)i};
            int result = sidecar(prox_native_flash_paths[i], ".prox-old", &job->before[i], remove_now, diagnostic);
            if (result < 0)
                return -1;
            found += result;
        }
    }
    if (job->worker_prepared) {
        *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_TEMP_WORKER, .part=-1};
        int result = sidecar(worker, ".prox-new", &job->worker, remove_now, diagnostic);
        if (result < 0)
            return -1;
        found += result;
    }
    return found;
}

int prox_native_cleanup_flash(ProxNativeCleanupMessage *job)
{
    const unsigned int mask = (1u << PROX_NI_FLASH_PARTS) - 1u;
    if (!job)
        return -1;
    ProxNativeCleanupDiagnostic *diagnostic = &job->diagnostic;
    *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_TARGET, .part=-1};
    if (sceKernelDevkitVersion() != 0x06060110u || sceKernelGetModel() != 1 || job->model != 1 ||
        !memchr(job->build_id, 0, sizeof(job->build_id)) || strcmp(job->build_id, PROX_RESIDENT_BUILD_ID) ||
        (job->prepared & ~mask) || (job->backups & ~job->prepared) || job->worker_prepared > 1)
        return failed(diagnostic, PROX_CLEANUP_CHECK_RECORD, 0);
    diagnostic->phase = PROX_CLEANUP_WORKER_IDENTITY;
    if (!valid_part(&job->worker, 1024u * 1024u) || job->worker.size != PROX_RESIDENT_IMAGE_SIZE ||
        strcmp(job->worker.sha256, PROX_RESIDENT_IMAGE_DIGEST))
        return failed(diagnostic, PROX_CLEANUP_CHECK_RECORD, (int)job->worker.size);
    for (unsigned int i = 0; i < PROX_NI_FLASH_PARTS; ++i) {
        *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_INSTALLED_PART, .part=(int)i};
        if (!valid_part(&job->installed[i], 1024u * 1024u))
            return failed(diagnostic, PROX_CLEANUP_CHECK_RECORD, 0);
        if ((job->backups & (1u << i)) && !valid_part(&job->before[i], 16u * 1024u * 1024u)) {
            diagnostic->phase = PROX_CLEANUP_BACKUP;
            return failed(diagnostic, PROX_CLEANUP_CHECK_RECORD, 0);
        }
        if (installed_matches(prox_native_flash_paths[i], &job->installed[i], diagnostic))
            return -1;
    }
    *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_INSTALLED_WORKER, .part=-1};
    if (installed_matches(PROX_RESIDENT_IMAGE_PATH, &job->worker, diagnostic))
        return -1;
    int found = sidecars(job, PROX_RESIDENT_IMAGE_PATH, 0);
    if (found <= 0)
        return found;

    ProxMountReport access = {0};
    int result = prox_native_mount_begin(&access);
    if (!result && access.active)
        result = sidecars(job, PROX_RESIDENT_IMAGE_PATH, 1) < 0 ? -1 : 0;
    else {
        *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_MOUNT, .part=-1, .detail=access.stage};
        result = -1;
    }
    if (!access.active)
        return result;
    if (prox_native_mount_end(&access) || !access.restored || access.active) {
        *diagnostic = (ProxNativeCleanupDiagnostic){.phase=PROX_CLEANUP_RESTORE, .part=-1, .detail=access.stage};
        return -2;
    }
    return result;
}
