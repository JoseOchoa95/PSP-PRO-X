/* GPL-3.0-or-later */
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <psputils.h>
#include <string.h>

#include "card_mount.h"
#include "dirs_card.h"
#include "fat32_dirs.h"
#include "fat32_probe.h"
#include "job.h"
#include "psp_media.h"
#include "sony_mkdir_ram.h"
#include "write_probe.h"

enum {
    FIRST_VOLUME_ID = 0x50585041, FAT_MOUNTED = 0x08, SECTORS_PER_GIB = 2097152,
    NO_STOCK_COMMAND = 1, PROBE_CONTROLS = 5,
};

static const char raw_device[] = "msstor:";
static struct FormatCard {
    struct MsAutoMedia measured;
    struct MsFormatMedia identity;
    struct MsFormatPreview preview;
    struct StockReader reader;
    struct StockFormatAccess *access;
    uint32_t metadata_start, metadata_length, started_us;
    int detached, probe_unsafe;
} card;
static SceUID raw = -1;

#ifdef MSWRITE_PATH_TRIAL
enum {
    WRITE_PROBE_LBA = 243271680,
    WRITE_PROBE_SECTORS = 1048576000,
};
#endif

int dirs_card_cleanup(void)
{
    if (raw < 0)
        return 1;
    if (sceIoClose(raw) < 0)
        return 0;
    raw = -1;
    return 1;
}

static int fat_is_idle(void)
{
    uint32_t state, mounts;
    return sony_fat_stock_state(&state, &mounts) && !mounts && !(state & FAT_MOUNTED);
}

static int read_locked(void *context, uint32_t lba, uint8_t *buffer)
{
    struct FormatCard *target = context;
    return fat_is_idle() && stock_format_locked_read(target->access, lba, buffer);
}

static int no_gpt(struct FormatCard *target)
{
    uint8_t sector[SECTOR_BYTES];
    return read_locked(target, 1, sector) && memcmp(sector, "EFI PART", 8) &&
           read_locked(target, target->measured.capacity.estimate.sectors - 1, sector) &&
           memcmp(sector, "EFI PART", 8);
}

static int inspect(void *context, struct MsFormatMedia *identity)
{
    struct FormatCard *target = context;
    if (target->access &&
        (!ms_card_matches(&target->measured.snapshot, 1, read_locked, target) || !no_gpt(target)))
        return 0;
    *identity = target->identity;
    return 1;
}

static int matching_insertion(struct FormatCard *target, int include_mbr)
{
    uint32_t reported;
    return !ms_auto_profile(raw, &target->reader, &reported, NULL) &&
           reported == target->identity.reported_sectors &&
           ms_card_matches(&target->measured.snapshot, include_mbr, ms_auto_read_sector, &target->reader);
}

struct LockedFormat {
    struct FormatCard *target;
    MsFormatLockedOperation operation;
    void *context;
    int admitted;
};

#ifndef MSWRITE_PATH_TRIAL
static int preflight_write(struct FormatCard *target);
#endif

static void run_locked(struct StockFormatAccess *access, void *context)
{
    struct LockedFormat *job = context;
    job->target->access = access;
#ifdef MSWRITE_PATH_TRIAL
    job->admitted = 1;
#else
    job->admitted = preflight_write(job->target);
#endif
    if (job->admitted)
        job->operation(job->context);
    job->target->access = NULL;
}

static int detach_stock_fat(struct FormatCard *target)
{
    uint32_t state, mounts;
    if (!sony_fat_stock_state(&state, &mounts)) {
        pspDebugScreenPrintf("Stock FAT context could not be read.\n");
        return 0;
    }
    pspDebugScreenPrintf("FAT before: mounts %u, state 0x%08x\n", mounts, state);
    if (mounts || (state & FAT_MOUNTED)) {
        int result = sceIoUnassign("ms0:");
        pspDebugScreenPrintf("Detach ms0: 0x%08x\n", (unsigned)result);
        if (result < 0)
            return 0;
        target->detached = 1;
    }
    if (!sony_fat_stock_state(&state, &mounts)) {
        pspDebugScreenPrintf("Stock FAT context changed.\n");
        return 0;
    }
    pspDebugScreenPrintf("FAT after: mounts %u, state 0x%08x\n", mounts, state);
    return !mounts && !(state & FAT_MOUNTED);
}

static enum MsFormatAccess exclusive(void *context, MsFormatLockedOperation operation, void *job_context)
{
    struct FormatCard *target = context;
    if (raw >= 0)
        return MS_FORMAT_ACCESS_CLEANUP_FAILED;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(target, 1);
    if (!dirs_card_cleanup())
        return MS_FORMAT_ACCESS_CLEANUP_FAILED;
    if (!matched) {
        pspDebugScreenPrintf("The card changed after the preview.\n");
        return MS_FORMAT_ACCESS_DENIED;
    }
    if (!sony_mkdir_ram_check()) {
        pspDebugScreenPrintf("Stock FAT module profile could not be loaded.\n");
        return MS_FORMAT_ACCESS_DENIED;
    }
    if (!detach_stock_fat(target)) {
        pspDebugScreenPrintf("Stock FAT did not enter its idle state.\n");
        return MS_FORMAT_ACCESS_DENIED;
    }

    raw = sceIoOpen(raw_device, PSP_O_RDWR, 0);
    int status = 0;
    if (matching_insertion(target, 1) &&
        ms_auto_check_boundary(&target->reader, target->identity.verified_sectors) && fat_is_idle()) {
        struct LockedFormat job = {.target = target, .operation = operation, .context = job_context};
        status = stock_format_transaction(&target->reader, run_locked, &job, target->measured.plan.usable_sectors);
        if (status > 0 && !job.admitted)
            status = 0;
    }
    if (!dirs_card_cleanup() || status < 0)
        return MS_FORMAT_ACCESS_CLEANUP_FAILED;
    if (!status)
        pspDebugScreenPrintf("The card did not pass exclusive write preflight.\n");
    return status ? MS_FORMAT_ACCESS_OK : MS_FORMAT_ACCESS_DENIED;
}

#ifndef MSWRITE_PATH_TRIAL
static int planned_write(const struct FormatCard *target, uint32_t lba)
{
    if (!lba || (lba >= target->metadata_start && lba - target->metadata_start < target->metadata_length))
        return 1;
    const struct MsFormatPlan *plan = &target->measured.plan;
    for (unsigned i = plan->primary_count; i < plan->volume_count; ++i) {
        struct MsFormatVolume volume;
        if (ms_format_volume(plan, i, &volume) && volume.ebr == lba)
            return 1;
    }
    return 0;
}

static int write_locked(void *context, uint32_t lba, const uint8_t *buffer)
{
    struct FormatCard *target = context;
    return planned_write(target, lba) &&
           ms_card_matches(&target->measured.snapshot, 0, read_locked, target) && fat_is_idle() &&
           stock_format_locked_write(target->access, lba, buffer);
}

static int stage(void *context, struct Fat32Ram *image, const struct MsFormatVolume *volume, unsigned index)
{
    struct FormatCard *target = context;
    struct Fat32Layout layout;
    if (!fat32_plan(volume->sectors, &layout))
        return 0;
    target->metadata_start = volume->start;
    target->metadata_length = layout.root_sector + (PSP_INITIAL_DIRECTORY_COUNT + 1) * layout.sectors_per_cluster;
    target->started_us = sceKernelGetSystemTimeLow();
    pspDebugScreenPrintf("P%u: format and verify FAT32 metadata\n", index + 1);
    return dirs_card_stage(image, volume->start, volume->sectors, FIRST_VOLUME_ID + index) &&
           fat32_ram_name_partition(image, index + 1);
}

static void verified(void *context, unsigned index, uint32_t writes)
{
    const struct FormatCard *target = context;
    unsigned seconds = (sceKernelGetSystemTimeLow() - target->started_us) / 1000000;
    pspDebugScreenPrintf("P%u verified: %u writes, %u seconds\n", index + 1, writes, seconds);
}

static unsigned show_mounted_partition(const struct MsFormatPlan *plan)
{
    struct Fat32VolumeIdentity mounted;
    SceIoStat game;
    if (sceIoGetstat("ms0:/PSP/GAME", &game) < 0 || !FIO_S_ISDIR(game.st_mode) ||
        !sony_fat_stock_volume(&mounted)) {
        pspDebugScreenPrintf("Mounted partition: unconfirmed\n");
        return 0;
    }
    const char *label = mounted.label[0] ? mounted.label : "PARTITION";
    uint32_t index = mounted.serial - FIRST_VOLUME_ID;
    struct MsFormatVolume expected;
    if (ms_format_volume(plan, index, &expected) && mounted.start == expected.start &&
        mounted.sectors == expected.sectors) {
        pspDebugScreenPrintf("Mounted P%u: %s   LBA %u\n", (unsigned)index + 1, label, (unsigned)mounted.start);
        return index + 1;
    }
    pspDebugScreenPrintf("Mounted FAT32: %s   LBA %u\n", label, (unsigned)mounted.start);
    return 0;
}

static int mount_first(void *context, const struct MsFormatMedia *identity, const struct MsFormatVolume *first)
{
    (void)identity;
    struct FormatCard *target = context;
    uint8_t expected[SECTOR_BYTES], actual[SECTOR_BYTES];
    if (raw >= 0 || !fat_is_idle() || !ms_format_table_sector(&target->measured.plan, 0, expected))
        return 0;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(target, 0) && ms_auto_read_sector(&target->reader, 0, actual) &&
                  !memcmp(actual + 444, expected + 444, SECTOR_BYTES - 444);
    int closed = dirs_card_cleanup();
    return matched && closed && ms_mount_first(&target->reader, first, FIRST_VOLUME_ID) &&
           show_mounted_partition(&target->measured.plan) == 1;
}
#endif

static void restore_original_mount(struct FormatCard *target)
{
    if (raw >= 0 || !fat_is_idle())
        return;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(target, 1);
    int closed = dirs_card_cleanup();
    if (matched && closed) {
        int result = sceIoAssign("ms0:", "msstor0p1:", "fatms0:", IOASSIGN_RDWR, NULL, 0);
        pspDebugScreenPrintf("Restore original ms0: 0x%08x\n", (unsigned)result);
    }
}

#ifndef MSWRITE_PATH_TRIAL
static const struct MsFormatBackend backend = {
    inspect, exclusive, read_locked, write_locked, stage, mount_first, verified,
};
#endif

static const char *prepare_preview(struct FormatCard *target)
{
    const char *error = ms_auto_measure(raw, &target->measured);
    if (error)
        return error;
    const struct MsAutoMedia *measured = &target->measured;
    pspDebugScreenPrintf("Adapter: %u sectors   Card: %u sectors\n",
                         measured->reported, measured->capacity.estimate.sectors);
    if (measured->gpt)
        return "GPT detected. GPT conversion is pending.";
#ifdef MSWRITE_PATH_TRIAL
    if (measured->capacity.estimate.sectors != WRITE_PROBE_SECTORS || !measured->reported ||
        WRITE_PROBE_LBA % measured->reported == 0)
        return "Only the prepared 500 GiB trial card can be tested.";
#endif
    if (measured->layout == MS_FORMAT_STOCK)
        return "Use the Sony format option in System Settings.";
    if (!measured->have_snapshot)
        return "The boundary or the two live witnesses were inconclusive.";
    target->identity.reported_sectors = measured->reported;
    target->identity.verified_sectors = measured->capacity.estimate.sectors;
    if (sceKernelUtilsSha1Digest((uint8_t *)&measured->snapshot, sizeof(measured->snapshot), target->identity.identity))
        return "Could not identify the inserted card.";
#ifndef MSWRITE_PATH_TRIAL
    if (ms_format_preview(&backend, target, &target->preview) != MS_FORMAT_EXTENDED)
        return "A FAT32 layout could not be calculated.";
#endif
    return NULL;
}

static void show_preview(const struct FormatCard *target)
{
#ifdef MSWRITE_PATH_TRIAL
    pspDebugScreenPrintf("\nOne-sector test at LBA %u.\nThe original sector will be restored.\n",
                         WRITE_PROBE_LBA);
    pspDebugScreenPrintf("Target is %s the driver-reported end.\n",
                         WRITE_PROBE_LBA >= target->measured.reported ? "above" : "within");
    pspDebugScreenPrintf("\nUse only the backed-up 512 GB trial card.\n"
                         "L + R + X: test write    O: cancel and exit\n");
#else
    const struct MsFormatPlan *plan = &target->preview.plan;
    uint32_t gib = target->identity.verified_sectors * UINT64_C(100) / SECTORS_PER_GIB;
    pspDebugScreenPrintf("Card: %u.%02u GiB. Planning cap: 1 TB.\n"
                         "%u FAT32 volumes (%u primary, %u logical)\n"
                         "Live witness LBAs: %u, %u\n\n",
                         gib / 100, gib % 100, plan->volume_count, plan->primary_count,
                         plan->volume_count - plan->primary_count,
                         target->measured.snapshot.witnesses[0].lba, target->measured.snapshot.witnesses[1].lba);
    unsigned first = 0;
    if (plan->volume_count > 7) {
        first = plan->volume_count - 1;
        pspDebugScreenPrintf("P1 to P%u: %u GiB each (%u sectors)\n",
                             first, plan->full_sectors / SECTORS_PER_GIB, plan->full_sectors);
    }
    for (unsigned i = first; i < plan->volume_count; ++i) {
        struct MsFormatVolume volume;
        if (!ms_format_volume(plan, i, &volume))
            break;
        gib = (uint64_t)volume.sectors * 100 / SECTORS_PER_GIB;
        pspDebugScreenPrintf("P%u: %u.%02u GiB  start %u  sectors %u\n",
                             i + 1, gib / 100, gib % 100, volume.start, volume.sectors);
    }
    pspDebugScreenPrintf("\nErases the inserted card. Keep it inserted.\n"
                         "L + R + X: format    O: cancel and exit\n");
#endif
}

int dirs_card_preview(void)
{
    if (!dirs_card_cleanup())
        return 0;
    memset(&card, 0, sizeof(card));
    pspDebugScreenPrintf("Scanning the inserted card. Keep it inserted.\n");
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    const char *error = prepare_preview(&card);
    if (!dirs_card_cleanup())
        error = "The read-only card handle could not close.";
#ifndef MSWRITE_PATH_TRIAL
    pspDebugScreenPrintf("\n");
    ms_mount_report();
    show_mounted_partition(&card.measured.plan);
#endif
    if (error) {
#ifndef MSWRITE_PATH_TRIAL
        ms_format_cancel(&card.preview);
#endif
        pspDebugScreenPrintf("\n%s\n\nX: check again    O: exit\n", error);
        return 0;
    }
    show_preview(&card);
    return 1;
}

struct WritePath {
    struct FormatCard *target;
    uint32_t lba, controls[PROBE_CONTROLS];
    uint8_t before[PROBE_CONTROLS][SECTOR_BYTES], original[SECTOR_BYTES];
    int attempt_command, restore_command, writes, controls_ok;
    struct MsWriteProbeResult result;
};

static int controls_match(struct WritePath *path)
{
    struct FormatCard *target = path->target;
    if (!ms_card_matches(&target->measured.snapshot, 1, read_locked, target))
        return 0;
    for (unsigned i = 0; i < PROBE_CONTROLS; ++i) {
        uint8_t now[SECTOR_BYTES];
        if (path->controls[i] && (!read_locked(target, path->controls[i], now) ||
            memcmp(now, path->before[i], sizeof(now))))
            return 0;
    }
    return 1;
}

static int probe_read(void *context, uint32_t lba, uint8_t *sector)
{
    struct WritePath *path = context;
    return lba == path->lba && read_locked(path->target, lba, sector);
}

static int probe_write(void *context, uint32_t lba, const uint8_t *sector)
{
    struct WritePath *path = context;
    struct FormatCard *target = path->target;
    if (lba != path->lba || !controls_match(path) || !fat_is_idle())
        return 0;
    int *status = path->writes++ ? &path->restore_command : &path->attempt_command;
    return stock_format_locked_write_status(target->access, lba, sector, status);
}

static void probe_locked(void *context)
{
    struct WritePath *path = context;
    struct FormatCard *target = path->target;
    struct MsFormatMedia identity;
    if (!inspect(target, &identity) || !read_locked(target, path->lba, path->original))
        return;
    path->controls[0] = path->lba != 2048 ? 2048 : 0;
    const uint32_t periods[] = {MS_PROBE_FOUR_GIB_SECTORS, DECIMAL_CUTOFF, BINARY_CUTOFF, target->measured.reported};
    for (unsigned i = 0; i < sizeof(periods) / sizeof(periods[0]); ++i) {
        if (path->lba >= periods[i])
            path->controls[i + 1] = path->lba % periods[i];
    }
    for (unsigned i = 0; i < PROBE_CONTROLS; ++i) {
        if (path->controls[i] && !read_locked(target, path->controls[i], path->before[i]))
            return;
    }
    path->result = ms_probe_sector(path->lba, probe_read, probe_write, path);
    path->controls_ok = controls_match(path);
}

#ifdef MSWRITE_PATH_TRIAL
static int reopen_matches(struct WritePath *path)
{
    struct FormatCard *target = path->target;
    uint8_t sector[SECTOR_BYTES];
    if (raw >= 0)
        return 0;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(target, 1) &&
                  ms_auto_read_sector(&target->reader, path->lba, sector) &&
                  !memcmp(sector, path->original, sizeof(sector));
    int closed = dirs_card_cleanup();
    return matched && closed;
}

void dirs_card_write(struct Fat32Ram *image)
{
    (void)image;
    struct WritePath path = {
        .target = &card, .lba = WRITE_PROBE_LBA,
        .attempt_command = NO_STOCK_COMMAND, .restore_command = NO_STOCK_COMMAND,
    };
    pspDebugScreenPrintf("Adapter: %u sectors\nTarget LBA: %u\n\n", card.measured.reported, WRITE_PROBE_LBA);
    enum MsFormatAccess access = exclusive(&card, probe_locked, &path);
    int reopened = access == MS_FORMAT_ACCESS_OK && path.result.restored &&
                   path.controls_ok && reopen_matches(&path);
    if (card.detached && (reopened || (!path.writes && access != MS_FORMAT_ACCESS_CLEANUP_FAILED)))
        restore_original_mount(&card);
    static const char *const readbacks[] = {"unavailable", "original bytes", "test pattern", "different bytes"};
    pspDebugScreenPrintf("Access: %u  Write attempts: %d\n", (unsigned)access, path.writes);
    if (path.attempt_command != NO_STOCK_COMMAND)
        pspDebugScreenPrintf("Pattern Sony result: 0x%08x\n", (unsigned)path.attempt_command);
    if (path.restore_command != NO_STOCK_COMMAND)
        pspDebugScreenPrintf("Restore Sony result: 0x%08x\n", (unsigned)path.restore_command);
    pspDebugScreenPrintf("Readback: %s\nControls: %s\nOriginal after reopen: %s\n\n",
                         readbacks[path.result.readback], path.controls_ok ? "MATCH" : "unconfirmed",
                         reopened ? "MATCH" : "unconfirmed");
    pspDebugScreenPrintf("Write path: %s\n\nO: exit\n",
                         reopened && path.result.accepted && path.result.readback == MS_PROBE_PATTERN ? "PASS" : "FAIL");
}
#else
static int preflight_write(struct FormatCard *target)
{
    uint32_t lba = ms_probe_high_metadata(&target->measured.plan, target->measured.reported);
    if (!lba)
        return 1;
    struct WritePath path = {
        .target = target, .lba = lba,
        .attempt_command = NO_STOCK_COMMAND, .restore_command = NO_STOCK_COMMAND,
    };
    probe_locked(&path);
    target->probe_unsafe = path.writes && (!path.result.restored || !path.controls_ok);
    if (path.result.accepted && path.result.readback == MS_PROBE_PATTERN &&
        path.result.restored && path.controls_ok) {
        pspDebugScreenPrintf("Extended write check: PASS (LBA %u)\n", lba);
        return 1;
    }
    pspDebugScreenPrintf("The card could not be formatted.\n"
                         "Could not verify writes to some sectors.\n"
                         "Write check failed at LBA %u.\n", lba);
    if (target->probe_unsafe)
        pspDebugScreenPrintf("Original sector or card controls could not be verified.\n");
    return 0;
}

void dirs_card_write(struct Fat32Ram *image)
{
    unsigned count = card.preview.plan.volume_count;
    pspDebugScreenPrintf("Formatting all volumes. Keep the card inserted.\n\n");
    struct MsFormatResult result = ms_format_execute(&backend, &card, &card.preview, image);
    if (card.detached && !card.probe_unsafe && !result.writes && result.failure != MS_FORMAT_COMPLETE &&
        result.failure != MS_FORMAT_RELEASE_FAILED)
        restore_original_mount(&card);
    if (result.sector_failure) {
        static const char *const errors[] = {"", "read failed", "write refused", "readback differs"};
        pspDebugScreenPrintf("LBA %u: %s\n", result.failure_lba, errors[result.sector_failure]);
    }
    pspDebugScreenPrintf("\nVerified partitions: %u/%u  Format writes: %u\n"
                         "Result code: %u\nAuto format trial: %s\n\nO: exit\n",
                         result.verified_volumes, count, result.writes, (unsigned)result.failure,
                         result.failure == MS_FORMAT_COMPLETE ? "PASS" : "FAIL");
}
#endif
