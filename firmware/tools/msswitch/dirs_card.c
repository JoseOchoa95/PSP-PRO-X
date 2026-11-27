/* GPL-3.0-or-later */
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspthreadman.h>
#include <psputils.h>
#include <string.h>

#include "dirs_card.h"
#ifdef MSMULTI_CARD_TRIAL
#include "card_mount.h"
#endif
#include "fat32_probe.h"
#include "partition_view.h"
#include "stock_reader.h"
#include "write_trial.h"

#ifdef MSMULTI_CARD_TRIAL
#include "capacity.h"
#include "fat32_dirs.h"
#include "job.h"
#include "sony_mkdir_ram.h"
#define REFERENCE_PATH "ms0:/PSP/GAME/MSMULTI/MSPART.BIN"
#else
#define REFERENCE_PATH "ms0:/PSP/GAME/MSDIRCARD/MSPART.BIN"
#endif
#define RAW_DEVICE "msstor:"
enum { RAW_USB_CAPACITY = 0x02125006, RAW_MEDIA_ATTRIBUTES = 0x02125803 };

static const uint32_t controls[] = {2048, 268435456, TRIAL_HIGH_LBA, 230688768, TRIAL_SECTORS - 1};
static const unsigned control_indices[] = {1, 2, 3, 4, 6};
static struct WriteTrialReference reference;
static struct StockReader reader;
static SceUID media = -1;
static unsigned char confirmed_boot[SECTOR_BYTES];
static unsigned char sector[SECTOR_BYTES] __attribute__((aligned(64)));
static int has_reference, confirmed;

int dirs_card_load_reference(void)
{
    SceUID file = sceIoOpen(REFERENCE_PATH, PSP_O_RDONLY, 0);
    if (file < 0)
        return 0;
    unsigned char extra;
    int bytes = sceIoRead(file, &reference, sizeof(reference));
    int tail = sceIoRead(file, &extra, 1);
    int closed = sceIoClose(file);
    has_reference = bytes == (int)sizeof(reference) && !tail && !closed &&
                    !memcmp(reference.magic, "MSPREF01", sizeof(reference.magic));
    return has_reference;
}

static int read_raw(uint32_t lba)
{
    int result = -1;
    return !stock_read_sector(&reader, lba, sector, &result, NULL) && result == SECTOR_BYTES;
}

static int root_accessible(const char *path)
{
    SceUID root = sceIoDopen(path);
    if (root < 0)
        return 0;
    sceIoDclose(root);
    return 1;
}

static const char *check_geometry(void)
{
    if (media < 0)
        return "Could not open the test card.";
    uint32_t blocks = 0;
    struct MediaGeometry geometry;
    unsigned char attributes[MEDIA_ATTRIBUTE_BYTES] __attribute__((aligned(64)));
    SceOff end = sceIoLseek(media, 0, PSP_SEEK_END);
    if (sceIoIoctl(media, RAW_USB_CAPACITY, NULL, 0, &blocks, sizeof(blocks)) < 0 ||
        blocks != TRIAL_REPORTED_BLOCKS || end != (SceOff)blocks * SECTOR_BYTES ||
        sceIoIoctl(media, RAW_MEDIA_ATTRIBUTES, NULL, 0, attributes, sizeof(attributes)) < 0 ||
        !decode_media_geometry(&geometry, attributes) || geometry.bytes != (uint64_t)end ||
        geometry.block_size != 4096 || geometry.unit_bytes != SECTOR_BYTES)
        return "Wrong card geometry or adapter.";
    return stock_reader_init(&reader, blocks);
}

static const char *check_card(void)
{
    const char *error = check_geometry();
    if (error)
        return error;
    if (root_accessible("ms0:/") || root_accessible("fatms0:/"))
        return "A stock Memory Stick volume is mounted.";

    struct PartitionView views[MBR_PARTITIONS];
    if (!read_raw(0) || memcmp(sector, reference.marker, SECTOR_BYTES) ||
        read_primary_views(sector, TRIAL_SECTORS, views) != 3 ||
        views[2].start != TRIAL_FORMAT_START || views[2].sectors != TRIAL_FORMAT_SECTORS)
        return "The MBR or partition 3 differs from the test reference.";
    for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        unsigned char digest[DIGEST_BYTES];
        if (!read_raw(controls[i]) || sceKernelUtilsSha1Digest(sector, SECTOR_BYTES, digest) ||
            memcmp(digest, reference.witnesses[control_indices[i]], sizeof(digest)))
            return "A card control sector differs from the test reference.";
    }

    static const unsigned char serial[] = {0x31, 0x50, 0x58, 0x50};
    if (!read_raw(TRIAL_FORMAT_START) || memcmp(sector + 3, "PROX    ", 8) ||
        memcmp(sector + 67, serial, sizeof(serial)) ||
        !fat32_matches_extent(sector, TRIAL_FORMAT_START, TRIAL_FORMAT_SECTORS))
        return "Partition 3 needs the previously verified PRO-X FAT32.";
    return stock_write_trial_check(&reader, NULL);
}

int dirs_card_cleanup(void)
{
    if (media < 0)
        return 1;
    if (sceIoClose(media) < 0)
        return 0;
    media = -1;
    return 1;
}

#ifdef MSMULTI_CARD_TRIAL
static struct MsFormatPreview preview;
static struct StockFormatAccess *access;
static uint32_t write_start, write_length;
static uint32_t started_us;
static int repeat_trial, detached_mount;

static int published_card(void);

static int stock_fat_is_idle(void)
{
    uint32_t state, mounts;
    return sony_fat_stock_state(&state, &mounts) && !mounts && !(state & 0x08);
}

static int matching_control(uint32_t lba, unsigned witness)
{
    uint8_t digest[DIGEST_BYTES];
    return stock_format_locked_read(access, lba, sector) &&
           !sceKernelUtilsSha1Digest(sector, SECTOR_BYTES, digest) &&
           !memcmp(digest, reference.witnesses[witness], sizeof(digest));
}

static enum CapacityReply probe_capacity(void *context, uint32_t lba)
{
    const struct StockReader *target = context;
    struct StockReadTrace trace;
    int result;
    if (stock_read_sector(target, lba, sector, &result, &trace) || !trace.calls)
        return CAPACITY_OTHER_FAILURE;
    if (result == SECTOR_BYTES && !trace.last_result)
        return CAPACITY_READ;
    if ((uint32_t)result == 0x8022008a && (uint32_t)trace.last_result == 0x8022008a)
        return CAPACITY_TRANSFER_FAILURE;
    return CAPACITY_OTHER_FAILURE;
}

static int inspect_card(void *context, struct MsFormatMedia *identity)
{
    (void)context;
    if (repeat_trial) {
        if (!published_card())
            return 0;
    } else if (access) {
        if (!stock_format_locked_read(access, 0, sector) || memcmp(sector, reference.marker, SECTOR_BYTES))
            return 0;
        for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
            if (!matching_control(controls[i], control_indices[i]))
                return 0;
        }
        if (!stock_format_locked_read(access, TRIAL_FORMAT_START, sector) ||
            memcmp(sector, confirmed_boot, SECTOR_BYTES))
            return 0;
    } else {
        if (check_card())
            return 0;
        memcpy(confirmed_boot, sector, sizeof(confirmed_boot));
        struct CapacityEstimate capacity = estimate_capacity(reader.reported_blocks, probe_capacity, &reader);
        if (capacity.finding != CAPACITY_CANDIDATE || capacity.sectors != TRIAL_SECTORS)
            return 0;
    }
    identity->reported_sectors = reader.reported_blocks;
    identity->verified_sectors = TRIAL_SECTORS;
    memcpy(identity->identity, reference.witnesses[3], DIGEST_BYTES);
    return 1;
}

struct WholeCardJob {
    MsFormatLockedOperation operation;
    void *context;
};

static void locked_job(struct StockFormatAccess *locked, void *context)
{
    struct WholeCardJob *job = context;
    access = locked;
    job->operation(job->context);
    access = NULL;
}

static enum MsFormatAccess exclusive_card(void *context, MsFormatLockedOperation operation, void *job_context)
{
    (void)context;
    if (repeat_trial) {
        /* Prove the requested card while it is still mounted. Only detach it
           after the caller confirms this exact layout. */
        media = sceIoOpen(RAW_DEVICE, PSP_O_RDONLY, 0);
        int recognized = published_card();
        if (!dirs_card_cleanup())
            return MS_FORMAT_ACCESS_CLEANUP_FAILED;
        if (!recognized) {
            pspDebugScreenPrintf("Preflight: formatted card identity changed.\n");
            return MS_FORMAT_ACCESS_DENIED;
        }
        uint32_t state, mounts;
        if (!sony_fat_stock_state(&state, &mounts)) {
            pspDebugScreenPrintf("Preflight: stock FAT state unavailable.\n");
            return MS_FORMAT_ACCESS_DENIED;
        }
        pspDebugScreenPrintf("FAT before: mounts %u, state 0x%08x\n", mounts, state);
        if (mounts || (state & 0x08)) {
            int detached = sceIoUnassign("ms0:");
            pspDebugScreenPrintf("Detach ms0: 0x%08x\n", (unsigned)detached);
            if (detached < 0)
                return MS_FORMAT_ACCESS_DENIED;
            detached_mount = 1;
        }
        if (!sony_fat_stock_state(&state, &mounts)) {
            pspDebugScreenPrintf("Preflight: cannot inspect FAT after detach.\n");
            return MS_FORMAT_ACCESS_DENIED;
        }
        pspDebugScreenPrintf("FAT after: mounts %u, state 0x%08x\n", mounts, state);
        if (mounts || (state & 0x08)) {
            pspDebugScreenPrintf("Preflight: stock FAT is still mounted.\n");
            return MS_FORMAT_ACCESS_DENIED;
        }
    }
    media = sceIoOpen(RAW_DEVICE, PSP_O_RDWR, 0);
    struct WholeCardJob job = {operation, job_context};
    int result = 0;
    if (media < 0) {
        pspDebugScreenPrintf("Raw write open: 0x%08x\n", (unsigned)media);
        return MS_FORMAT_ACCESS_DENIED;
    }
    int matches = repeat_trial ? published_card() :
                                !check_card() && !memcmp(sector, confirmed_boot, sizeof(confirmed_boot));
    const char *write_error = matches ? stock_write_trial_check(&reader, NULL) : "Raw card identity changed.";
    if (write_error) {
        pspDebugScreenPrintf("Preflight: %s\n", write_error);
    } else {
        result = stock_format_transaction(&reader, locked_job, &job, TRIAL_SECTORS);
        if (!result)
            pspDebugScreenPrintf("Preflight: stock worker did not acquire access.\n");
    }
    if (!dirs_card_cleanup() || result < 0)
        return MS_FORMAT_ACCESS_CLEANUP_FAILED;
    return result ? MS_FORMAT_ACCESS_OK : MS_FORMAT_ACCESS_DENIED;
}

static int read_card(void *context, uint32_t lba, uint8_t *buffer)
{
    (void)context;
    return (!repeat_trial || stock_fat_is_idle()) && stock_format_locked_read(access, lba, buffer);
}

static int write_card(void *context, uint32_t lba, const uint8_t *buffer)
{
    (void)context;
    /* The three-volume trial has no EBR. Each volume can change only its metadata. */
    if (lba && (lba < write_start || lba - write_start >= write_length))
        return 0;
    if (repeat_trial && !stock_fat_is_idle())
        return 0;
    return matching_control(TRIAL_HIGH_LBA, 3) && matching_control(268435456, 2) &&
           stock_format_locked_write(access, lba, buffer);
}

static int stage_card(void *context, struct Fat32Ram *image, const struct MsFormatVolume *target,
                      unsigned index)
{
    (void)context;
    struct Fat32Layout fat;
    if (target->ebr || !fat32_plan(target->sectors, &fat))
        return 0;
    write_start = target->start;
    write_length = fat.root_sector + (PSP_INITIAL_DIRECTORY_COUNT + 1) * fat.sectors_per_cluster;
    started_us = sceKernelGetSystemTimeLow();
    pspDebugScreenPrintf("Partition %u: prepare and verify FAT32 metadata\n", index + 1);
    return dirs_card_stage(image, target->start, target->sectors, 0x50585041 + index);
}

static void show_volume_result(void *context, unsigned index, uint32_t writes)
{
    (void)context;
    unsigned seconds = (sceKernelGetSystemTimeLow() - started_us) / 1000000;
    pspDebugScreenPrintf("P%u verified: %u writes, %u seconds\n", index + 1, writes, seconds);
}

static int mount_partition_one(void *context, const struct MsFormatMedia *identity,
                               const struct MsFormatVolume *first)
{
    (void)context;
    (void)identity;
    return ms_mount_first(&reader, first, 0x50585041);
}

static const struct MsFormatBackend backend = {inspect_card, exclusive_card, read_card, write_card,
                                               stage_card, mount_partition_one, show_volume_result};

static int read_trial_sector(uint32_t lba)
{
    return access ? stock_format_locked_read(access, lba, sector) : read_raw(lba);
}

static int published_card(void)
{
    if ((!access && check_geometry()) || !read_trial_sector(0))
        return 0;
    struct MsFormatPlan plan;
    uint8_t expected[SECTOR_BYTES];
    if (ms_format_layout(reader.reported_blocks, TRIAL_SECTORS, &plan) != MS_FORMAT_EXTENDED ||
        plan.volume_count != 3 || plan.primary_count != 3 || !ms_format_table_sector(&plan, 0, expected))
        return 0;
    memcpy(expected + 440, reference.marker + 440, 4);
    if (memcmp(sector, expected, sizeof(expected)))
        return 0;
    const unsigned stable[] = {1, 2, 4};
    for (unsigned i = 0; i < sizeof(stable) / sizeof(stable[0]); ++i) {
        unsigned entry = stable[i];
        unsigned char digest[DIGEST_BYTES];
        if (!read_trial_sector(controls[entry]) || sceKernelUtilsSha1Digest(sector, SECTOR_BYTES, digest) ||
            memcmp(digest, reference.witnesses[control_indices[entry]], sizeof(digest)))
            return 0;
    }
    for (unsigned i = 0; i < plan.volume_count; ++i) {
        struct MsFormatVolume target;
        uint8_t serial[] = {0x41 + i, 0x50, 0x58, 0x50};
        if (!ms_format_volume(&plan, i, &target) || !read_trial_sector(target.start) ||
            !fat32_matches_extent(sector, target.start, target.sectors) || memcmp(sector + 67, serial, 4))
            return 0;
    }
    return 1;
}

void dirs_card_test_mount(void)
{
    if (!has_reference || media >= 0)
        return;
    media = sceIoOpen(RAW_DEVICE, PSP_O_RDONLY, 0);
    int matched = published_card();
    if (!dirs_card_cleanup() || !matched) {
        pspDebugScreenPrintf("Published trial volumes could not be verified.\n\nO: exit\n");
        return;
    }
    struct MsFormatVolume first = {2048, 230686720, 0};
    pspDebugScreenPrintf("Three formatted volumes and witnesses: MATCH\n\n");
    int mounted = mount_partition_one(NULL, NULL, &first);
    pspDebugScreenPrintf("\nPartition 1 mount trial: %s\n\nO: exit\n", mounted ? "PASS" : "FAIL");
}

static int preview_card(int repeat)
{
    confirmed = 0;
    repeat_trial = repeat;
    detached_mount = 0;
    if (!has_reference || !dirs_card_cleanup())
        return 0;
    media = sceIoOpen(RAW_DEVICE, PSP_O_RDONLY, 0);
    enum MsFormatLayoutResult result = ms_format_preview(&backend, NULL, &preview);
    int can_format = result == MS_FORMAT_EXTENDED && preview.plan.volume_count == 3 &&
                     preview.plan.primary_count == 3;
    int already_formatted = !can_format && !repeat && published_card();
    if (!dirs_card_cleanup()) {
        ms_format_cancel(&preview);
        pspDebugScreenPrintf("Could not close the card handle.\nO: exit\n");
        return 0;
    }
    if (!can_format) {
        ms_format_cancel(&preview);
        if (already_formatted)
            pspDebugScreenPrintf("All three formatted volumes were recognized.\n"
                                 "Start: repeat format    Square: test mount    O: exit\n");
        else
            pspDebugScreenPrintf("The trial card or layout could not be verified.\n"
                                 "X: check again    Square: test mount    O: exit\n");
        return 0;
    }
    pspDebugScreenPrintf("Verified 256 GB trial card. Planning cap: 1 TB.\n\n");
    for (unsigned i = 0; i < preview.plan.volume_count; ++i) {
        struct MsFormatVolume target;
        if (!ms_format_volume(&preview.plan, i, &target))
            return 0;
        uint32_t gib_hundredths = (uint64_t)target.sectors * 100 / 2097152;
        pspDebugScreenPrintf("P%u: %u.%02u GiB  LBA %u through %u\n", i + 1,
                             gib_hundredths / 100, gib_hundredths % 100,
                             target.start, target.start + target.sectors - 1);
    }
    pspDebugScreenPrintf("\nErases all three volumes of the backed-up trial card.\n"
                         "L + R + X: format    O: cancel and exit\n");
    confirmed = 1;
    return 1;
}

int dirs_card_preview(void)
{
    return preview_card(0);
}

int dirs_card_preview_repeat(void)
{
    return preview_card(1);
}

void dirs_card_write(struct Fat32Ram *image)
{
    if (!confirmed || media >= 0)
        return;
    confirmed = 0;
    pspDebugScreenPrintf("Formatting all volumes. Keep the card inserted.\n\n");
    struct MsFormatResult result = ms_format_execute(&backend, NULL, &preview, image);
    if (detached_mount && !result.writes && result.failure != MS_FORMAT_COMPLETE && media < 0 &&
        stock_fat_is_idle())
        dirs_card_test_mount();
    pspDebugScreenPrintf("\nVerified partitions: %u/3   Sector writes: %u\n", result.verified_volumes, result.writes);
    pspDebugScreenPrintf("Result code: %u\n", (unsigned)result.failure);
    pspDebugScreenPrintf("Whole card trial: %s\n\nO: exit\n", result.failure == MS_FORMAT_COMPLETE ? "PASS" : "FAIL");
}
#else
int dirs_card_preview(void)
{
    confirmed = 0;
    if (!has_reference)
        return 0;
    if (!dirs_card_cleanup()) {
        pspDebugScreenPrintf("Could not close the previous card handle.\nO: exit\n");
        return 0;
    }
    media = sceIoOpen(RAW_DEVICE, PSP_O_RDONLY, 0);
    const char *error = check_card();
    if (!error)
        memcpy(confirmed_boot, sector, sizeof(confirmed_boot));
    if (!dirs_card_cleanup())
        error = "Could not close the read-only card handle.";
    if (error) {
        pspDebugScreenPrintf("%s\n\nX: check again    O: exit\n", error);
        return 0;
    }
    confirmed = 1;
    pspDebugScreenPrintf("Verified backed-up 256 GB card and original adapter.\n\n"
                         "Partition 3: LBA %u through %u\n"
                         "Write range:  LBA %u through %u\n"
                         "Replaces partition 3's FAT32 and its contents.\n"
                         "The new volume has 17 folders and two indicator files.\n\n"
                         "L + R + X: write    O: cancel and exit\n",
                         TRIAL_FORMAT_START, TRIAL_FORMAT_START + TRIAL_FORMAT_SECTORS - 1,
                         TRIAL_FORMAT_START, TRIAL_FORMAT_START + TRIAL_DIRECTORY_METADATA_SECTORS - 1);
    return 1;
}

void dirs_card_write(struct Fat32Ram *image)
{
    if (!confirmed || media >= 0)
        return;
    confirmed = 0;
    struct StockFormatTrial trial = {
        .reader = &reader, .reference = &reference, .confirmed_boot = confirmed_boot,
        .worker_result = -1, .directory_image = image,
    };
    media = sceIoOpen(RAW_DEVICE, PSP_O_RDWR, 0);
    const char *error = check_card();
    if (!error && memcmp(confirmed_boot, sector, sizeof(confirmed_boot)))
        error = "The FAT32 boot sector changed since the preview.";
    int passed = 0;
    if (!error) {
        pspDebugScreenPrintf("Checking and updating partition 3. Keep the card inserted.\n");
        passed = stock_format_partition_trial(&trial);
        error = trial.error;
    }
    int closed = dirs_card_cleanup();
    if (error)
        pspDebugScreenPrintf("%s\n", error);
    pspDebugScreenPrintf("Sector writes: %u, verified sectors: %u\n"
                         "Worker: 0x%08x  command: 0x%08x\n"
                         "Cleanup: %s\n\nPhysical directory trial: %s\n\nO: exit\n",
                         trial.writes, trial.verified, (unsigned)trial.worker_result,
                         (unsigned)trial.command_result, closed ? "PASS" : "FAIL",
                         passed && closed ? "PASS" : "FAIL");
}
#endif
