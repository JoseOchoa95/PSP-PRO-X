/* GPL-3.0-or-later */
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <psputils.h>
#include <string.h>

#include "dirs_card.h"
#include "fat32_probe.h"
#include "partition_view.h"
#include "stock_reader.h"
#include "write_trial.h"

#define REFERENCE_PATH "ms0:/PSP/GAME/MSDIRCARD/MSPART.BIN"
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

static const char *check_card(void)
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
    const char *error = stock_reader_init(&reader, blocks);
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
