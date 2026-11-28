/* GPL-3.0-or-later */
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <psputils.h>
#include <string.h>

#include "card_mount.h"
#include "format_output.h"
#include "dirs_card.h"
#include "fat32_dirs.h"
#include "fat32_probe.h"
#include "auto_format.h"
#include "psp_media.h"
#include "ram_volume.h"
#include "sony_mkdir_ram.h"
#include "write_probe.h"
#ifndef MSWRITE_PATH_TRIAL
#include <pspmscm.h>
#include "blank_capacity.h"
#include "media_session.h"
#endif
#ifdef MSBLANK_TRIAL
#include "blank_probe.h"
#endif

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
#ifndef MSWRITE_PATH_TRIAL
    unsigned read_batches;
    int batch_unavailable;
    enum MsMediaSession session;
    int blank_pending;
    struct MsBlankCapacity blank_check;
#endif
    int detached, probe_unsafe;
} card;
static SceUID raw = -1;

#ifdef MSBLANK_TRIAL
enum { BLANK_TEST_COUNT = 2, BLANK_CARD_SECTORS = 500695040, BLANK_ADAPTER_SECTORS = 232259584 };
static struct MsBlankProbe blank_probes[BLANK_TEST_COUNT];
static struct MsBlankSamples blank_samples[BLANK_TEST_COUNT];
#elif defined(MSWRITE_PATH_TRIAL)
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

#ifndef MSWRITE_PATH_TRIAL
static SceUID media_callback = -1;
static int media_monitor_registered;

static int medium_changed(int count, int event, void *context)
{
    (void)count;
    (void)event;
    ms_session_notify(context);
    return 0;
}

int dirs_card_stop_monitor(void)
{
    if (media_monitor_registered) {
        if (MScmUnregisterMSInsertEjectCallback(media_callback) < 0)
            return 0;
        media_monitor_registered = 0;
        ms_session_notify(&card.session);
    }
    if (media_callback >= 0 && sceKernelDeleteCallback(media_callback) < 0)
        return 0;
    media_callback = -1;
    return 1;
}

int dirs_card_start_monitor(void)
{
    if (media_monitor_registered)
        return 1;
    if (media_callback < 0)
        media_callback = sceKernelCreateCallback("PRO-X format media", medium_changed, &card.session);
    if (media_callback < 0 || MScmRegisterMSInsertEjectCallback(media_callback) != 0)
        return 0;
    media_monitor_registered = 1;
    return 1;
}

static const char *start_review(struct FormatCard *target)
{
    if (!dirs_card_start_monitor())
        return "Could not register the card-change monitor.";
    int presence = MScmIsMediumInserted();
    /* Registration can queue an initial event for the card already present. */
    sceKernelCheckCallback();
    if (!ms_session_mark(&target->session, media_monitor_registered, presence))
        return "Could not confirm the card is present.";
    return NULL;
}

/* Service pending callbacks inside the stock transaction without device queries. */
static int review_current(const struct FormatCard *target)
{
    sceKernelCheckCallback();
    return target->session == MS_SESSION_CURRENT;
}

static int review_present(struct FormatCard *target)
{
    if (!review_current(target))
        return 0;
    ms_session_observe(&target->session, MScmIsMediumInserted());
    return review_current(target);
}

void ms_auto_format_discard(void)
{
    ms_session_notify(&card.session);
    card.blank_pending = 0;
    ms_format_cancel(&card.preview);
}

int ms_auto_format_current(void)
{
    if (review_present(&card))
        return 1;
    ms_auto_format_discard();
    return 0;
}
#endif

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

static int partition_marks_match(struct FormatCard *target)
{
    if (!target->identity.gpt.sectors_per_copy)
        return no_gpt(target);
    struct MsGptLayout live;
    return ms_gpt_inspect(&target->measured.plan, read_locked, target, &live) &&
           !memcmp(&live, &target->identity.gpt, sizeof(live));
}

static int inspect(void *context, struct MsFormatMedia *identity)
{
    struct FormatCard *target = context;
    if (target->access &&
        (!ms_card_matches(&target->measured.snapshot, 1, read_locked, target) || !partition_marks_match(target)))
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
    job->admitted = review_current(job->target) && partition_marks_match(job->target) &&
                    (job->target->blank_pending || preflight_write(job->target)) && review_current(job->target);
#endif
    if (job->admitted)
        job->operation(job->context);
    job->target->access = NULL;
}

static int detach_stock_fat(struct FormatCard *target)
{
    uint32_t state, mounts;
    if (!sony_fat_stock_state(&state, &mounts)) {
        format_trace("Stock FAT context could not be read.\n");
        return 0;
    }
    format_trace("FAT before: mounts %u, state 0x%08x\n", mounts, state);
    if (mounts || (state & FAT_MOUNTED)) {
        int result = sceIoUnassign("ms0:");
        format_trace("Detach ms0: 0x%08x\n", (unsigned)result);
        if (result < 0)
            return 0;
        target->detached = 1;
    }
    if (!sony_fat_stock_state(&state, &mounts)) {
        format_trace("Stock FAT context changed.\n");
        return 0;
    }
    format_trace("FAT after: mounts %u, state 0x%08x\n", mounts, state);
    return !mounts && !(state & FAT_MOUNTED);
}

static enum MsFormatAccess exclusive(void *context, MsFormatLockedOperation operation, void *job_context)
{
    struct FormatCard *target = context;
#ifndef MSWRITE_PATH_TRIAL
    if (!review_present(target) || (target->blank_pending && !fat_is_idle()))
        return MS_FORMAT_ACCESS_DENIED;
#endif
    if (raw >= 0)
        return MS_FORMAT_ACCESS_CLEANUP_FAILED;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(target, 1);
    if (!dirs_card_cleanup())
        return MS_FORMAT_ACCESS_CLEANUP_FAILED;
    if (!matched) {
        format_trace("The card changed after the preview.\n");
        return MS_FORMAT_ACCESS_DENIED;
    }
    if (!sony_mkdir_ram_check()) {
        format_trace("Stock FAT module profile could not be loaded.\n");
        return MS_FORMAT_ACCESS_DENIED;
    }
#ifndef MSWRITE_PATH_TRIAL
    if (!review_present(target))
        return MS_FORMAT_ACCESS_DENIED;
#endif
    if (!detach_stock_fat(target)) {
        format_trace("Stock FAT did not enter its idle state.\n");
        return MS_FORMAT_ACCESS_DENIED;
    }

    raw = sceIoOpen(raw_device, PSP_O_RDWR, 0);
    int status = 0;
    if (matching_insertion(target, 1) &&
        ms_auto_check_boundary(&target->reader, target->identity.verified_sectors) && fat_is_idle()
#ifndef MSWRITE_PATH_TRIAL
        && review_present(target)
#endif
    ) {
        struct LockedFormat job = {.target = target, .operation = operation, .context = job_context};
        status = stock_format_transaction(&target->reader, run_locked, &job, target->measured.plan.usable_sectors);
        if (status > 0 && !job.admitted)
            status = 0;
    }
    if (!dirs_card_cleanup() || status < 0)
        return MS_FORMAT_ACCESS_CLEANUP_FAILED;
    if (!status)
        format_trace("The card did not pass exclusive write preflight.\n");
    return status ? MS_FORMAT_ACCESS_OK : MS_FORMAT_ACCESS_DENIED;
}

#ifndef MSWRITE_PATH_TRIAL
_Static_assert(MS_FORMAT_READ_BATCH_SECTORS <= (int)STOCK_FORMAT_READ_BATCH_SECTORS,
               "format read batch fits the stock transfer buffer");

static int read_metadata_batch(void *context, uint32_t lba, unsigned count, uint8_t *buffer)
{
    struct FormatCard *target = context;
    if (target->batch_unavailable || lba < target->metadata_start ||
        lba - target->metadata_start >= target->metadata_length ||
        count > target->metadata_length - (lba - target->metadata_start) || !fat_is_idle())
        return 0;
    if (!stock_format_locked_read_batch(target->access, lba, count, buffer) || !fat_is_idle()) {
        target->batch_unavailable = 1;
        return 0;
    }
    ++target->read_batches;
    return 1;
}

static int planned_write(const struct FormatCard *target, uint32_t lba)
{
    if (!lba || ms_gpt_contains(&target->identity.gpt, lba) ||
        (lba >= target->metadata_start && lba - target->metadata_start < target->metadata_length))
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
           review_current(target) && stock_format_locked_write(target->access, lba, buffer);
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
    format_trace("P%u: format and verify FAT32 metadata\n", index + 1);
    return ms_ram_volume_stage(image, volume->start, volume->sectors, FIRST_VOLUME_ID + index) &&
           fat32_ram_name_partition(image, index + 1);
}

static void verified(void *context, unsigned index, uint32_t writes)
{
    const struct FormatCard *target = context;
    unsigned seconds = (sceKernelGetSystemTimeLow() - target->started_us) / 1000000;
    format_trace("P%u verified: %u writes, %u seconds\n", index + 1, writes, seconds);
}

static unsigned show_mounted_partition(const struct MsFormatPlan *plan)
{
    struct Fat32VolumeIdentity mounted;
    SceIoStat game;
    if (sceIoGetstat("ms0:/PSP/GAME", &game) < 0 || !FIO_S_ISDIR(game.st_mode) ||
        !sony_fat_stock_volume(&mounted)) {
        format_trace("Mounted partition: unconfirmed\n");
        return 0;
    }
    const char *label = mounted.label[0] ? mounted.label : "PARTITION";
    uint32_t index = mounted.serial - FIRST_VOLUME_ID;
    struct MsFormatVolume expected;
    if (ms_format_volume(plan, index, &expected) && mounted.start == expected.start &&
        mounted.sectors == expected.sectors) {
        format_trace("Mounted P%u: %s   LBA %u\n", (unsigned)index + 1, label, (unsigned)mounted.start);
        return index + 1;
    }
    format_trace("Mounted FAT32: %s   LBA %u\n", label, (unsigned)mounted.start);
    return 0;
}

static int mount_first(void *context, const struct MsFormatMedia *identity, const struct MsFormatVolume *first)
{
    (void)identity;
    struct FormatCard *target = context;
    uint8_t expected[SECTOR_BYTES], actual[SECTOR_BYTES];
    if (raw >= 0 || !fat_is_idle() || !review_present(target) ||
        !ms_format_table_sector(&target->measured.plan, 0, expected))
        return 0;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(target, 0) && ms_auto_read_sector(&target->reader, 0, actual) &&
                  !memcmp(actual + 444, expected + 444, SECTOR_BYTES - 444);
    int closed = dirs_card_cleanup();
    return matched && closed && review_present(target) && ms_mount_first(&target->reader, first, FIRST_VOLUME_ID) &&
           show_mounted_partition(&target->measured.plan) == 1;
}
#endif

static void restore_original_mount(struct FormatCard *target)
{
#ifndef MSWRITE_PATH_TRIAL
    if (!review_present(target))
        return;
#endif
    if (raw >= 0 || !fat_is_idle())
        return;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(target, 1);
    int closed = dirs_card_cleanup();
    if (matched && closed
#ifndef MSWRITE_PATH_TRIAL
        && review_present(target)
#endif
    ) {
        int result = sceIoAssign("ms0:", "msstor0p1:", "fatms0:", IOASSIGN_RDWR, NULL, 0);
        format_trace("Restore original ms0: 0x%08x\n", (unsigned)result);
    }
}

#ifndef MSWRITE_PATH_TRIAL
static const struct MsFormatBackend backend = {
    inspect, exclusive, read_locked, write_locked, stage, mount_first, verified, read_metadata_batch,
};
#endif

#ifdef MSBLANK_TRIAL
static int prepare_blank_samples(struct FormatCard *target)
{
    const uint32_t starts[BLANK_TEST_COUNT] = {294799177, 461375552};
    for (unsigned i = 0; i < BLANK_TEST_COUNT; ++i) {
        int found = 0;
        /* Start with the sectors verified by the direct reader and allow a
           small window when later use changes their contents. */
        for (uint32_t offset = 0; offset < 32; ++offset) {
            struct MsBlankProbe *probe = &blank_probes[i];
            if (ms_blank_probe_plan(target->measured.reported, BLANK_CARD_SECTORS, starts[i] + offset, probe) &&
                ms_blank_probe_capture(probe, &blank_samples[i], ms_auto_read_sector, &target->measured.reader)) {
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }
    return ms_card_matches(&target->measured.snapshot, 1, ms_auto_read_sector, &target->measured.reader);
}
#endif

static const char *prepare_preview(struct FormatCard *target, int allow_capacity_trial)
{
    const char *error = ms_auto_measure(raw, &target->measured);
    if (error)
        return error;
    struct MsAutoMedia *measured = &target->measured;
    format_trace("Adapter: %u sectors   Card: %u sectors\n",
                         measured->reported, measured->capacity.estimate.sectors);
#ifdef MSWRITE_PATH_TRIAL
    (void)allow_capacity_trial;
    if (measured->gpt)
        return "GPT is not supported by the write-path trial.";
#ifdef MSBLANK_TRIAL
    if (measured->capacity.estimate.sectors != BLANK_CARD_SECTORS || measured->reported != BLANK_ADAPTER_SECTORS)
        return "Use the backed-up 256 GB card and its original adapter.";
#else
    if (measured->capacity.estimate.sectors != WRITE_PROBE_SECTORS || !measured->reported ||
        WRITE_PROBE_LBA % measured->reported == 0)
        return "Only the prepared 500 GiB trial card can be tested.";
#endif
#endif
    if (measured->layout == MS_FORMAT_STOCK)
        return NULL;
#ifndef MSWRITE_PATH_TRIAL
    if (measured->gpt) {
        const struct AutoCapacity *capacity = &measured->capacity;
        if (measured->layout != MS_FORMAT_EXTENDED || capacity->estimate.finding != CAPACITY_CANDIDATE ||
            capacity->estimate.sectors != measured->plan.usable_sectors || !capacity->alias_checks ||
            capacity->distinct_checks != capacity->alias_checks)
            return "GPT detected. The usable boundary is inconclusive.";
        if (!ms_gpt_inspect(&measured->plan, ms_auto_read_sector, &measured->reader, &target->identity.gpt))
            return "GPT metadata is damaged or unsupported.";
        measured->have_snapshot = ms_card_capture(&measured->plan, &target->identity.gpt, measured->reported,
                                                   ms_auto_read_sector, &measured->reader, &measured->snapshot);
    } else if (allow_capacity_trial && !measured->have_snapshot && measured->layout == MS_FORMAT_EXTENDED &&
               measured->capacity.estimate.finding == CAPACITY_CANDIDATE && measured->capacity.alias_checks &&
               !measured->capacity.distinct_checks && measured->capacity.estimate.sectors == measured->plan.usable_sectors) {
        target->blank_pending = ms_blank_capacity_capture(&measured->plan, measured->reported,
                                                          ms_auto_read_sector, &measured->reader,
                                                          &target->blank_check, &measured->snapshot);
    }
    if (!measured->have_snapshot && !target->blank_pending)
#else
    if (!measured->have_snapshot)
#endif
        return "The boundary or the two live witnesses were inconclusive.";
    target->identity.reported_sectors = measured->reported;
    target->identity.verified_sectors = measured->capacity.estimate.sectors;
    if (sceKernelUtilsSha1Digest((uint8_t *)&measured->snapshot, sizeof(measured->snapshot), target->identity.identity))
        return "Could not identify the inserted card.";
#ifdef MSBLANK_TRIAL
    if (!prepare_blank_samples(target))
        return "Could not find two safe zero-filled high sectors.";
#endif
#ifndef MSWRITE_PATH_TRIAL
    if (!target->blank_pending && ms_format_preview(&backend, target, &target->preview) != MS_FORMAT_EXTENDED)
        return "A FAT32 layout could not be calculated.";
#endif
    return NULL;
}

#ifndef PROX_FORMAT_RESIDENT
static void show_preview(const struct FormatCard *target)
{
#ifdef MSBLANK_TRIAL
    (void)target;
    for (unsigned i = 0; i < BLANK_TEST_COUNT; ++i)
        format_trace("Sample %u: LBA %u, %u lower controls\n", i + 1, blank_probes[i].high,
                             blank_probes[i].count);
    format_trace("\nTemporary writes on the backed-up 256 GB card.\n"
                         "Original sectors must verify after restoration.\n"
                         "Keep the card inserted.\nL + R + X: test    O: cancel and exit\n");
#elif defined(MSWRITE_PATH_TRIAL)
    format_trace("\nOne-sector test at LBA %u.\nThe original sector will be restored.\n",
                         WRITE_PROBE_LBA);
    format_trace("Target is %s the driver-reported end.\n",
                         WRITE_PROBE_LBA >= target->measured.reported ? "above" : "within");
    format_trace("\nUse only the backed-up 512 GB trial card.\n"
                         "L + R + X: test write    O: cancel and exit\n");
#else
    if (target->blank_pending) {
        format_trace("\nCapacity candidate: %u sectors\n"
                             "The sampled sectors returned zeros.\n"
                             "Temporarily writes 2 sectors, then restores them.\n"
                             "Use only a backed-up card. Keep it inserted.\n\n"
                             "L + R + X: test sectors    O: cancel and exit\n",
                             target->measured.capacity.estimate.sectors);
        return;
    }
    const struct MsFormatPlan *plan = &target->preview.plan;
    uint32_t gib = target->identity.verified_sectors * UINT64_C(100) / SECTORS_PER_GIB;
    if (target->identity.gpt.sectors_per_copy)
        format_trace("GPT verified. Format replaces it with MBR.\n");
    format_trace("Card: %u.%02u GiB. Planning cap: 1 TB.\n"
                         "%u FAT32 volumes (%u primary, %u logical)\n"
                         "%s LBAs: %u, %u\n\n",
                         gib / 100, gib % 100, plan->volume_count, plan->primary_count,
                         plan->volume_count - plan->primary_count,
                         target->measured.have_snapshot ? "Live witness" : "Restored test",
                         target->measured.snapshot.witnesses[0].lba, target->measured.snapshot.witnesses[1].lba);
    unsigned first = 0;
    if (plan->volume_count > 7) {
        first = plan->volume_count - 1;
        format_trace("P1 to P%u: %u GiB each (%u sectors)\n",
                             first, plan->full_sectors / SECTORS_PER_GIB, plan->full_sectors);
    }
    for (unsigned i = first; i < plan->volume_count; ++i) {
        struct MsFormatVolume volume;
        if (!ms_format_volume(plan, i, &volume))
            break;
        gib = (uint64_t)volume.sectors * 100 / SECTORS_PER_GIB;
        format_trace("P%u: %u.%02u GiB  start %u  sectors %u\n",
                             i + 1, gib / 100, gib % 100, volume.start, volume.sectors);
    }
    format_trace("\nErases the inserted card. Keep it inserted.\n"
                         "L + R + X: format    O: cancel and exit\n");
#endif
}
#endif

static const char *scan_card(int allow_capacity_trial)
{
    if (!dirs_card_cleanup())
        return "The previous card handle could not close.";
    memset(&card, 0, sizeof(card));
    const char *error = NULL;
#ifndef MSWRITE_PATH_TRIAL
    error = start_review(&card);
#endif
    if (!error) {
        raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
        error = prepare_preview(&card, allow_capacity_trial);
    }
    if (!dirs_card_cleanup())
        error = "The read-only card handle could not close.";
    return error;
}

#ifndef MSWRITE_PATH_TRIAL
enum MsFormatLayoutResult ms_auto_format_scan(struct MsFormatPreview *preview)
{
    if (!preview)
        return MS_FORMAT_INVALID;
    ms_format_cancel(preview);
    if (scan_card(0) || !ms_auto_format_current()) {
        ms_auto_format_discard();
        return MS_FORMAT_INVALID;
    }
    *preview = card.preview;
    return card.measured.layout;
}
#endif

#ifndef PROX_FORMAT_RESIDENT
int dirs_card_preview(void)
{
    format_trace("Scanning the inserted card. Keep it inserted.\n");
    const char *error = scan_card(1);
    if (!error && card.measured.layout == MS_FORMAT_STOCK)
        error = "Use the Sony format option in System Settings.";
#ifndef MSWRITE_PATH_TRIAL
    if (card.session != MS_SESSION_UNCHECKED) {
        if (review_present(&card)) {
            format_trace("\n");
            ms_mount_report();
            show_mounted_partition(&card.measured.plan);
        }
        if (!review_present(&card) && !error)
            error = "Card changed during the scan. Scan it again.";
    }
#endif
    if (error) {
#ifndef MSWRITE_PATH_TRIAL
        ms_format_cancel(&card.preview);
#endif
        format_trace("\n%s\n\nX: check again    O: exit\n", error);
        return 0;
    }
    show_preview(&card);
    return 1;
}

#ifndef MSWRITE_PATH_TRIAL
_Static_assert(MS_FORMAT_WITNESSES == 2, "each capacity sample checks the other sample");

struct BlankCapacityTrial {
    struct FormatCard *target;
    uint32_t active_lba, peer_lba;
    unsigned passed;
    int peer_unchanged;
};

static int capacity_trial_present(void *context)
{
    struct BlankCapacityTrial *trial = context;
    struct FormatCard *target = trial->target;
    return target->access && review_current(target) && fat_is_idle() &&
           ms_blank_capacity_headers_zero(read_locked, target) && review_current(target);
}

static int capacity_trial_read(void *context, uint32_t lba, uint8_t *sector)
{
    struct BlankCapacityTrial *trial = context;
    return read_locked(trial->target, lba, sector);
}

static int capacity_trial_write(void *context, uint32_t lba, const uint8_t *sector)
{
    struct BlankCapacityTrial *trial = context;
    if (lba != trial->active_lba || !capacity_trial_present(trial))
        return 0;
    int accepted = stock_format_locked_write(trial->target->access, lba, sector);
    if (trial->peer_lba) {
        static const uint8_t zero[SECTOR_BYTES] = {0};
        uint8_t peer[SECTOR_BYTES];
        trial->peer_unchanged = read_locked(trial->target, trial->peer_lba, peer) && !memcmp(peer, zero, sizeof(peer));
        /* The temporary write still needs restoration if its peer changed. */
        trial->peer_lba = 0;
    }
    return accepted;
}

static void run_capacity_trial(void *context)
{
    struct BlankCapacityTrial *trial = context;
    const struct MsBlankCapacity *check = &trial->target->blank_check;
    if (!ms_blank_capacity_restored(check, read_locked, trial->target))
        return;
    for (unsigned i = 0; i < MS_FORMAT_WITNESSES; ++i) {
        trial->active_lba = check->probes[i].high;
        trial->peer_lba = check->probes[1 - i].high;
        trial->peer_unchanged = 0;
        struct MsBlankResult result = ms_blank_probe_run(&check->probes[i], &check->samples[i], capacity_trial_read,
                                                        capacity_trial_write, capacity_trial_present, trial);
        format_trace("LBA %u: result %u, writes %u, peer %s\n", trial->active_lba,
                             (unsigned)result.finding, result.writes, trial->peer_unchanged ? "MATCH" : "unverified");
        if (result.finding != MS_BLANK_INDEPENDENT || !trial->peer_unchanged)
            break;
        ++trial->passed;
    }
    trial->active_lba = 0;
}

static int capacity_trial_reopen(struct FormatCard *target)
{
    if (raw >= 0 || !review_present(target))
        return 0;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matches = matching_insertion(target, 1) &&
                  ms_blank_capacity_restored(&target->blank_check, ms_auto_read_sector, &target->reader) &&
                  ms_auto_check_boundary(&target->reader, target->identity.verified_sectors);
    return dirs_card_cleanup() && matches && review_present(target);
}

int dirs_card_needs_qualification(void)
{
    return card.blank_pending;
}

int dirs_card_qualify(void)
{
    struct BlankCapacityTrial trial = {.target = &card};
    if (!card.blank_pending)
        return 0;
    format_trace("Testing sampled sectors. Keep the card inserted.\n\n");
    enum MsFormatAccess access = exclusive(&card, run_capacity_trial, &trial);
    int restored = access == MS_FORMAT_ACCESS_OK && capacity_trial_reopen(&card);
    card.blank_pending = 0;
    if (!restored || trial.passed != MS_FORMAT_WITNESSES ||
        ms_format_preview(&backend, &card, &card.preview) != MS_FORMAT_EXTENDED) {
        ms_format_cancel(&card.preview);
        format_trace("\nOriginal samples: %s\nCapacity check failed. Format unavailable.\n\nO: exit\n",
                             restored ? "verified" : "unverified");
        return 0;
    }
    format_trace("\nCapacity check: PASS. Original samples restored.\n\n");
    show_preview(&card);
    return 1;
}
#endif
#endif

#ifdef MSBLANK_TRIAL
struct LockedBlankTrial {
    struct FormatCard *target;
    const struct MsBlankProbe *active;
    struct MsBlankResult results[BLANK_TEST_COUNT];
    unsigned tested;
};

static int blank_card_present(void *context)
{
    struct LockedBlankTrial *trial = context;
    struct MsFormatMedia live;
    return trial->target->access && fat_is_idle() && inspect(trial->target, &live);
}

static int blank_read(void *context, uint32_t lba, uint8_t *sector)
{
    struct LockedBlankTrial *trial = context;
    return read_locked(trial->target, lba, sector);
}

static int blank_write(void *context, uint32_t lba, const uint8_t *sector)
{
    struct LockedBlankTrial *trial = context;
    return trial->active && lba == trial->active->high && blank_card_present(trial) &&
           stock_format_locked_write_status(trial->target->access, lba, sector, NULL);
}

static void run_blank_trial(void *context)
{
    struct LockedBlankTrial *trial = context;
    for (unsigned i = 0; i < BLANK_TEST_COUNT; ++i) {
        trial->active = &blank_probes[i];
        trial->results[i] = ms_blank_probe_run(trial->active, &blank_samples[i], blank_read, blank_write,
                                               blank_card_present, trial);
        ++trial->tested;
        if (trial->results[i].finding != MS_BLANK_INDEPENDENT)
            break;
    }
    trial->active = NULL;
}

static int blank_reopen(unsigned tested)
{
    if (!tested || raw >= 0)
        return 0;
    raw = sceIoOpen(raw_device, PSP_O_RDONLY, 0);
    int matched = matching_insertion(&card, 1);
    for (unsigned i = 0; i < tested && matched; ++i)
        matched = ms_blank_probe_matches(&blank_probes[i], &blank_samples[i], ms_auto_read_sector, &card.reader);
    return dirs_card_cleanup() && matched;
}

void dirs_card_write(struct Fat32Ram *image)
{
    (void)image;
    struct LockedBlankTrial trial = {.target = &card};
    enum MsFormatAccess access = exclusive(&card, run_blank_trial, &trial);
    int reopened = access == MS_FORMAT_ACCESS_OK && blank_reopen(trial.tested);
    int success = reopened && trial.tested == BLANK_TEST_COUNT;
    unsigned attempts = 0;
    static const char *const findings[] = {"refused", "independent, restored", "alias detected, restored",
                                           "write rejected or dropped", "restoration unverified"};
    for (unsigned i = 0; i < trial.tested; ++i) {
        const struct MsBlankResult *result = &trial.results[i];
        format_trace("Sample %u LBA %u: %s\n", i + 1, blank_probes[i].high, findings[result->finding]);
        for (unsigned j = 0; j < blank_probes[i].count; ++j)
            format_trace("  Control LBA %u\n", blank_probes[i].controls[j]);
        if (result->alias_lba)
            format_trace("  Alias LBA %u\n", result->alias_lba);
        success &= result->finding == MS_BLANK_INDEPENDENT;
        attempts += result->writes;
    }
    if (card.detached && (reopened || (!attempts && access != MS_FORMAT_ACCESS_CLEANUP_FAILED)))
        restore_original_mount(&card);
    format_trace("Write attempts: %u, access: %u\nOriginal bytes after reopen: %s\n\n"
                         "Uniform sector trial: %s\n\nO: exit\n", attempts, (unsigned)access,
                         reopened ? "MATCH" : "unconfirmed", success ? "PASS" : "FAIL");
}
#else
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
#ifndef MSWRITE_PATH_TRIAL
    if (!review_current(target))
        return 0;
#endif
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
    format_trace("Adapter: %u sectors\nTarget LBA: %u\n\n", card.measured.reported, WRITE_PROBE_LBA);
    enum MsFormatAccess access = exclusive(&card, probe_locked, &path);
    int reopened = access == MS_FORMAT_ACCESS_OK && path.result.restored &&
                   path.controls_ok && reopen_matches(&path);
    if (card.detached && (reopened || (!path.writes && access != MS_FORMAT_ACCESS_CLEANUP_FAILED)))
        restore_original_mount(&card);
    static const char *const readbacks[] = {"unavailable", "original bytes", "test pattern", "different bytes"};
    format_trace("Access: %u  Write attempts: %d\n", (unsigned)access, path.writes);
    if (path.attempt_command != NO_STOCK_COMMAND)
        format_trace("Pattern Sony result: 0x%08x\n", (unsigned)path.attempt_command);
    if (path.restore_command != NO_STOCK_COMMAND)
        format_trace("Restore Sony result: 0x%08x\n", (unsigned)path.restore_command);
    format_trace("Readback: %s\nControls: %s\nOriginal after reopen: %s\n\n",
                         readbacks[path.result.readback], path.controls_ok ? "MATCH" : "unconfirmed",
                         reopened ? "MATCH" : "unconfirmed");
    format_trace("Write path: %s\n\nO: exit\n",
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
        format_trace("Extended write check: PASS (LBA %u)\n", lba);
        return 1;
    }
    format_trace("The card could not be formatted.\n"
                         "Could not verify writes to some sectors.\n"
                         "Write check failed at LBA %u.\n", lba);
    if (target->probe_unsafe)
        format_trace("Original sector or card controls could not be verified.\n");
    return 0;
}

struct MsFormatResult ms_auto_format_execute(struct MsFormatPreview *confirmed, struct Fat32Ram *image)
{
    struct MsFormatResult result = {.failure = MS_FORMAT_NO_PREVIEW};
    if (!card.blank_pending && card.preview.plan.volume_count &&
        ms_format_preview_equal(confirmed, &card.preview) && ms_auto_format_current())
        result = ms_format_execute(&backend, &card, confirmed, image);
    ms_format_cancel(confirmed);
    ms_format_cancel(&card.preview);
    if (card.detached && !card.probe_unsafe && !result.writes && result.failure != MS_FORMAT_COMPLETE &&
        result.failure != MS_FORMAT_RELEASE_FAILED)
        restore_original_mount(&card);
    card.detached = 0;
    return result;
}

#ifndef PROX_FORMAT_RESIDENT
void dirs_card_write(struct Fat32Ram *image)
{
    if (card.blank_pending)
        return;
    unsigned count = card.preview.plan.volume_count;
    format_trace("Formatting all volumes. Keep the card inserted.\n\n");
    struct MsFormatResult result = ms_auto_format_execute(&card.preview, image);
    if (card.session == MS_SESSION_INVALIDATED)
        format_trace("The card changed. The format attempt was stopped.\n");
    if (result.sector_failure) {
        static const char *const errors[] = {"", "read failed", "write refused", "readback differs"};
        format_trace("LBA %u: %s\n", result.failure_lba, errors[result.sector_failure]);
    }
    format_trace("\nMetadata read batches: %u%s\n", card.read_batches,
                         card.batch_unavailable ? " (single-sector fallback)" : "");
    if (card.identity.gpt.sectors_per_copy)
        format_trace("GPT sectors cleared: %u/%u\n", result.gpt_sectors_cleared,
                             2 * card.identity.gpt.sectors_per_copy);
    format_trace("Verified partitions: %u/%u  Format writes: %u\n"
                         "Result code: %u\nAuto format trial: %s\n\nO: exit\n",
                         result.verified_volumes, count, result.writes, (unsigned)result.failure,
                         result.failure == MS_FORMAT_COMPLETE ? "PASS" : "FAIL");
}
#endif

#endif
#endif
