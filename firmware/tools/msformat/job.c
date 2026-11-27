/* GPL-3.0-or-later */
#include <string.h>

#include "fat32_dirs.h"
#include "job.h"

enum { BACKUP_BOOT = 6 };

enum MsFormatLayoutResult ms_format_preview(const struct MsFormatBackend *backend, void *device,
                                            struct MsFormatPreview *preview)
{
    if (!preview)
        return MS_FORMAT_INVALID;
    ms_format_cancel(preview);
    if (!backend || !backend->inspect || !backend->inspect(device, &preview->media))
        return MS_FORMAT_INVALID;
    return ms_format_layout(preview->media.reported_sectors, preview->media.verified_sectors,
                            &preview->plan);
}

void ms_format_cancel(struct MsFormatPreview *preview)
{
    if (preview)
        memset(preview, 0, sizeof(*preview));
}

static int matches_preview(const struct MsFormatBackend *backend, void *device,
                           const struct MsFormatPreview *preview)
{
    struct MsFormatMedia live = {0};
    struct MsFormatPlan plan;
    const struct MsFormatPlan *expected = &preview->plan;
    return backend->inspect(device, &live) &&
           live.reported_sectors == preview->media.reported_sectors &&
           live.verified_sectors == preview->media.verified_sectors &&
           !memcmp(live.identity, preview->media.identity, sizeof(live.identity)) &&
           ms_format_layout(live.reported_sectors, live.verified_sectors, &plan) == MS_FORMAT_EXTENDED &&
           plan.usable_sectors == expected->usable_sectors && plan.full_sectors == expected->full_sectors &&
           plan.final_sectors == expected->final_sectors && plan.volume_count == expected->volume_count &&
           plan.primary_count == expected->primary_count;
}

static uint32_t stage(const struct MsFormatBackend *backend, void *device, struct Fat32Ram *image,
                      const struct MsFormatVolume *volume, unsigned index)
{
    memset(image, 0, sizeof(*image));
    struct Fat32Layout fat;
    if (!fat32_plan(volume->sectors, &fat) ||
        !backend->stage(device, image, volume, index) || image->start != volume->start ||
        image->length != volume->sectors || image->used > FAT32_RAM_CAPACITY)
        return 0;
    uint32_t metadata = fat.root_sector + (PSP_INITIAL_DIRECTORY_COUNT + 1) * fat.sectors_per_cluster;
    if (metadata > volume->sectors)
        return 0;
    for (unsigned i = 0; i < image->used; ++i) {
        if (image->entries[i].lba >= metadata)
            return 0;
    }
    return fat32_ram_finish_directories(image) ? metadata : 0;
}

static int write_sector(const struct MsFormatBackend *backend, void *device, uint32_t lba,
                        const uint8_t *sector, struct MsFormatResult *result)
{
    ++result->writes;
    if (backend->write(device, lba, sector))
        return 1;
    result->sector_failure = MS_SECTOR_WRITE_FAILED;
    result->failure_lba = lba;
    return 0;
}

static int read_sector(const struct MsFormatBackend *backend, void *device, uint32_t lba,
                       uint8_t *sector, struct MsFormatResult *result)
{
    if (backend->read(device, lba, sector))
        return 1;
    result->sector_failure = MS_SECTOR_READ_FAILED;
    result->failure_lba = lba;
    return 0;
}

static int verify_sector(const struct MsFormatBackend *backend, void *device, uint32_t lba,
                         const uint8_t *expected, struct MsFormatResult *result)
{
    uint8_t actual[FAT32_SECTOR_BYTES];
    if (!read_sector(backend, device, lba, actual, result))
        return 0;
    if (!memcmp(actual, expected, sizeof(actual)))
        return 1;
    result->sector_failure = MS_SECTOR_MISMATCH;
    result->failure_lba = lba;
    return 0;
}

static int write_and_verify(const struct MsFormatBackend *backend, void *device, uint32_t lba,
                            const uint8_t *expected, struct MsFormatResult *result)
{
    return write_sector(backend, device, lba, expected, result) &&
           verify_sector(backend, device, lba, expected, result);
}

static int write_volume(const struct MsFormatBackend *backend, void *device, struct Fat32Ram *image,
                        uint32_t metadata, struct MsFormatResult *result)
{
    uint8_t expected[FAT32_SECTOR_BYTES] = {0}, actual[FAT32_SECTOR_BYTES];
    uint32_t start = image->start;
    if (!write_and_verify(backend, device, start, expected, result) ||
        !write_and_verify(backend, device, start + BACKUP_BOOT, expected, result))
        return 0;
    int verified_change = 0;
    for (uint32_t lba = 1; lba < metadata; ++lba) {
        if (lba == BACKUP_BOOT)
            continue;
        uint32_t absolute = start + lba;
        if (!fat32_ram_read(image, lba, expected) || !read_sector(backend, device, absolute, actual, result))
            return 0;
        if (!memcmp(actual, expected, sizeof(actual)))
            continue;
        if (!write_sector(backend, device, absolute, expected, result) ||
            (!verified_change && !verify_sector(backend, device, absolute, expected, result)))
            return 0;
        verified_change = 1;
    }
    if (!fat32_ram_read(image, BACKUP_BOOT, expected) ||
        !write_and_verify(backend, device, start + BACKUP_BOOT, expected, result) ||
        !fat32_ram_read(image, 0, expected) || !write_and_verify(backend, device, start, expected, result))
        return 0;
    for (uint32_t lba = 0; lba < metadata; ++lba) {
        if (!fat32_ram_read(image, lba, expected) || !verify_sector(backend, device, start + lba, expected, result))
            return 0;
    }
    return 1;
}

static int publish_table(const struct MsFormatBackend *backend, void *device,
                         const struct MsFormatPlan *plan, uint32_t lba, const uint8_t signature[4],
                         struct MsFormatResult *result)
{
    uint8_t expected[FAT32_SECTOR_BYTES];
    if (!ms_format_table_sector(plan, lba, expected))
        return 0;
    if (!lba)
        memcpy(expected + 440, signature, 4);
    return write_and_verify(backend, device, lba, expected, result);
}

static enum MsFormatFailure format_locked(const struct MsFormatBackend *backend, void *device,
                                          const struct MsFormatPreview *preview, struct Fat32Ram *scratch,
                                          struct MsFormatResult *result)
{
    if (!matches_preview(backend, device, preview))
        return MS_FORMAT_MEDIA_CHANGED;
    const struct MsFormatPlan *plan = &preview->plan;
    struct MsFormatVolume volume;
    uint8_t signature[4];
    for (unsigned i = 0; i < plan->volume_count; ++i) {
        if (!ms_format_volume(plan, i, &volume))
            return MS_FORMAT_NO_PREVIEW;
        uint32_t metadata = stage(backend, device, scratch, &volume, i);
        if (!metadata)
            return MS_FORMAT_INITIAL_CONTENTS;
        if (!i) {
            /* Hide the old layout before changing volumes. Publish the new MBR
               after all boot records and EBRs have passed readback. */
            uint8_t empty[FAT32_SECTOR_BYTES] = {0};
            uint8_t actual[FAT32_SECTOR_BYTES];
            if (!read_sector(backend, device, 0, actual, result))
                return MS_FORMAT_TABLE_IO;
            if (actual[510] == 0x55 && actual[511] == 0xaa && memcmp(actual + 440, empty, 4)) {
                memcpy(signature, actual + 440, sizeof(signature));
            } else {
                memcpy(signature, preview->media.identity, sizeof(signature));
                signature[0] |= 1;
            }
            if (!write_and_verify(backend, device, 0, empty, result))
                return MS_FORMAT_TABLE_IO;
        }
        uint32_t writes_before = result->writes;
        if (!write_volume(backend, device, scratch, metadata, result))
            return MS_FORMAT_VOLUME_IO;
        ++result->verified_volumes;
        if (backend->volume_verified)
            backend->volume_verified(device, i, result->writes - writes_before);
    }
    for (unsigned i = plan->volume_count; i > plan->primary_count; --i) {
        if (!ms_format_volume(plan, i - 1, &volume) ||
            !publish_table(backend, device, plan, volume.ebr, signature, result))
            return MS_FORMAT_TABLE_IO;
    }
    return publish_table(backend, device, plan, 0, signature, result) ? MS_FORMAT_COMPLETE : MS_FORMAT_TABLE_IO;
}

struct LockedJob {
    const struct MsFormatBackend *backend;
    void *device;
    const struct MsFormatPreview *approved;
    struct Fat32Ram *scratch;
    struct MsFormatResult *result;
};

static void run_locked(void *context)
{
    struct LockedJob *job = context;
    job->result->failure = format_locked(job->backend, job->device, job->approved, job->scratch,
                                         job->result);
}

struct MsFormatResult ms_format_execute(const struct MsFormatBackend *backend, void *device,
                                        struct MsFormatPreview *preview, struct Fat32Ram *scratch)
{
    struct MsFormatResult result = {.failure = MS_FORMAT_NO_PREVIEW};
    if (!preview || !preview->plan.volume_count)
        return result;
    struct MsFormatPreview approved = *preview;
    ms_format_cancel(preview);
    if (!scratch || !backend || !backend->inspect || !backend->exclusive || !backend->read ||
        !backend->write || !backend->stage || !backend->mount_first) {
        result.failure = MS_FORMAT_ACCESS;
        return result;
    }
    struct LockedJob job = {backend, device, &approved, scratch, &result};
    result.failure = MS_FORMAT_ACCESS;
    enum MsFormatAccess access = backend->exclusive(device, run_locked, &job);
    if (access == MS_FORMAT_ACCESS_CLEANUP_FAILED) {
        result.failure = MS_FORMAT_RELEASE_FAILED;
    } else if (access != MS_FORMAT_ACCESS_OK) {
        result.failure = MS_FORMAT_ACCESS;
    } else if (result.failure == MS_FORMAT_COMPLETE) {
        struct MsFormatVolume first;
        if (!ms_format_volume(&approved.plan, 0, &first) ||
            !backend->mount_first(device, &approved.media, &first))
            result.failure = MS_FORMAT_FIRST_MOUNT;
    }
    return result;
}
