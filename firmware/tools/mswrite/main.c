/* Standalone, single-sector Memory Stick write trial. GPL-3.0-or-later. */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psputils.h>
#include <string.h>
#include <systemctrl.h>

#include "stock_reader.h"
#include "write_trial.h"

#ifdef MSPART_TRIAL
#include "partition_table.h"
PSP_MODULE_INFO("ProxMsPartTrial", PSP_MODULE_KERNEL, 1, 0);
#define TRIAL_TITLE "PRO-X Memory Stick Partition Trial"
#define TRIAL_FILE "MSPART.BIN"
#define TRIAL_PATH "ms0:/PSP/GAME/MSPART/" TRIAL_FILE
#define WITNESSES_AFTER_WRITE 1
#else
PSP_MODULE_INFO("ProxMsWriteTrial", PSP_MODULE_KERNEL, 1, 0);
#define TRIAL_TITLE "PRO-X Memory Stick Write Trial"
#define TRIAL_FILE "MSWTEST.BIN"
#define TRIAL_PATH "ms0:/PSP/GAME/MSWRITE/" TRIAL_FILE
#define WITNESSES_AFTER_WRITE 0
#endif

PSP_MAIN_THREAD_ATTR(0);
#define RAW_DEVICE "msstor:"
#define RAW_USB_CAPACITY 0x02125006
#define RAW_MEDIA_ATTRIBUTES 0x02125803
#define CONFIRM_BUTTONS (PSP_CTRL_CROSS | PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)

static const uint32_t witnesses[TRIAL_WITNESSES] = {
#ifdef MSPART_TRIAL
    0, 2048, BINARY_CUTOFF, TRIAL_HIGH_LBA, 230688768, 461375488, TRIAL_SECTORS - 1,
#else
    0, 2048, BINARY_CUTOFF, TRIAL_LBA % (1u << 23), TRIAL_LBA % DECIMAL_CUTOFF,
    TRIAL_LBA % BINARY_CUTOFF, TRIAL_LBA % TRIAL_REPORTED_BLOCKS,
#endif
};

static struct WriteTrialReference reference;
static unsigned char sector[SECTOR_BYTES] __attribute__((aligned(64)));
static unsigned char marker[SECTOR_BYTES] __attribute__((aligned(64)));
#ifdef MSPART_TRIAL
static struct TrialPartition layout[TRIAL_PARTITIONS];
#endif

static int load_reference(void)
{
    SceUID fd = sceIoOpen(TRIAL_PATH, PSP_O_RDONLY, 0);
    unsigned char extra;
    int size, tail, closed;
    if (fd < 0)
        return 0;
    size = sceIoRead(fd, &reference, sizeof(reference));
    tail = sceIoRead(fd, &extra, 1);
    closed = sceIoClose(fd);
    if (size != (int)sizeof(reference) || tail != 0 || closed < 0 ||
        memcmp(reference.magic, TRIAL_MAGIC, 8) ||
        !memcmp(reference.original, reference.marker, SECTOR_BYTES))
        return 0;
#ifdef MSPART_TRIAL
    return build_trial_mbr(reference.original, marker, layout) &&
           !memcmp(marker, reference.marker, SECTOR_BYTES);
#else
    memcpy(marker, reference.marker, sizeof(marker));
    return 1;
#endif
}

static int read_sector(const struct StockReader *reader, uint32_t lba)
{
    int result = -1;
    return !stock_read_sector(reader, lba, sector, &result, NULL) && result == SECTOR_BYTES;
}

#ifdef MSPART_TRIAL
static int matches_witnesses(const struct StockReader *reader, unsigned start)
#else
static int matches_witnesses(const struct StockReader *reader)
#endif
{
    unsigned char digest[DIGEST_BYTES];
    unsigned i;
#ifndef MSPART_TRIAL
    const unsigned start = 0;
#endif
    for (i = start; i < TRIAL_WITNESSES; i++) {
        if (!read_sector(reader, witnesses[i]) ||
            sceKernelUtilsSha1Digest(sector, sizeof(sector), digest) != 0 ||
            memcmp(digest, reference.witnesses[i], sizeof(digest)))
            return 0;
        if (i == 0 && (primary_mbr_end(sector) != TRIAL_PARTITION_END ||
                       sector[450] != 0x07))
            return 0;
    }
    return 1;
}

static const char *check_geometry(SceUID fd, struct StockReader *reader)
{
    struct MediaGeometry geometry;
    uint8_t attributes[MEDIA_ATTRIBUTE_BYTES] __attribute__((aligned(64)));
    uint32_t blocks;
    SceOff end;
    if (fd < 0)
        return "Could not open the test card.";
    end = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (sceIoIoctl(fd, RAW_USB_CAPACITY, NULL, 0, &blocks, sizeof(blocks)) < 0 ||
        blocks != TRIAL_REPORTED_BLOCKS || end != (SceOff)blocks * SECTOR_BYTES ||
        sceIoIoctl(fd, RAW_MEDIA_ATTRIBUTES, NULL, 0, attributes, sizeof(attributes)) < 0 ||
        !decode_media_geometry(&geometry, attributes) || geometry.bytes != (uint64_t)end ||
        geometry.block_size != 4096 || geometry.unit_bytes != SECTOR_BYTES)
        return "Unexpected reported geometry.";
    return stock_reader_init(reader, blocks);
}

static const char *check_contents(const struct StockReader *reader)
{
#ifdef MSPART_TRIAL
    if (!matches_witnesses(reader, 0))
#else
    if (!matches_witnesses(reader))
#endif
        return "The test card's control sectors do not match.";
    if (!read_sector(reader, TRIAL_LBA) || memcmp(sector, reference.original, SECTOR_BYTES))
        return "The target sector differs from its PC backup.";
    return NULL;
}

static int can_open_volume(const char *path)
{
    SceUID directory = sceIoDopen(path);
    if (directory < 0)
        return 0;
    sceIoDclose(directory);
    return 1;
}

static int volume_is_open(void)
{
    return can_open_volume("ms0:/") || can_open_volume("fatms0:/");
}

static const char *show_access_state(const struct StockReader *reader, const char *label)
{
    struct StockWriteState state = {0};
    const char *error = stock_write_trial_check(reader, &state);
    if (state.observed) {
        pspDebugScreenPrintf("%s state: type 0x%02x, handles %u\n"
                             "  Write enabled %u, protected %u\n",
                             label, (unsigned)state.media_type, (unsigned)state.open_handles,
                             (unsigned)state.write_enabled, (unsigned)state.write_protected);
    }
    if (error)
        pspDebugScreenPrintf("  %s\n", error);
    return error;
}

static const char *inspect_open(SceUID fd, const char *label, const char **blocker)
{
    struct StockReader reader;
    const char *error = check_geometry(fd, &reader);
    if (error)
        return error;
    *blocker = show_access_state(&reader, label);
    return check_contents(&reader);
}

static const char *preview(void)
{
    const char *error, *blocker = NULL;
    SceUID fd;
    if (volume_is_open())
        return "A mounted Memory Stick volume was found.";
    fd = sceIoOpen(RAW_DEVICE, PSP_O_RDONLY, 0);
    error = inspect_open(fd, "Read-only", &blocker);
    if (fd >= 0 && sceIoClose(fd) < 0)
        return "Read-only device close failed.";
    if (error)
        return error;
    if (volume_is_open())
        return "A Memory Stick volume mounted during the preview.";
    /* O_RDWR without create or truncate only asks the stock driver for access.
       The trial write remains behind a fresh confirmation. */
    fd = sceIoOpen(RAW_DEVICE, PSP_O_RDWR, 0);
    if (fd < 0) {
        pspDebugScreenPrintf("Stock read/write open failed: 0x%08x\n", (unsigned)fd);
        return blocker ? blocker : "Stock driver refused write access.";
    }
    pspDebugScreenPrintf("Stock read/write open accepted.\n");
    error = inspect_open(fd, "Read/write", &blocker);
    if (sceIoClose(fd) < 0)
        return "Read/write device close failed.";
    return error ? error : blocker;
}

static void run_write(void)
{
    struct StockReader reader;
    SceUID fd;
    int result = -1, dispatched = 0;
    const char *error;
    if (volume_is_open()) {
        pspDebugScreenPrintf("A mounted volume was found. Cancelled.\n");
        return;
    }
    fd = sceIoOpen(RAW_DEVICE, PSP_O_RDWR, 0);
    error = check_geometry(fd, &reader);
    if (!error)
        error = check_contents(&reader);
    if (!error)
        error = stock_write_trial(&reader, reference.original, marker, reference.witnesses[0],
                                  &result, &dispatched);
    if (error)
        pspDebugScreenPrintf("%s\n", error);
    pspDebugScreenPrintf("Write dispatched %d, result 0x%08x\n", dispatched, (unsigned)result);
    if (dispatched) {
        int readback = read_sector(&reader, TRIAL_LBA) && !memcmp(sector, marker, sizeof(marker));
        pspDebugScreenPrintf("PSP readback %s\n", readback ? "MATCH" : "FAILED");
#ifdef MSPART_TRIAL
        int witness_match = matches_witnesses(&reader, WITNESSES_AFTER_WRITE);
#else
        int witness_match = matches_witnesses(&reader);
#endif
        pspDebugScreenPrintf("Witness sectors %s\n", witness_match ? "match" : "changed/unreadable");
    }
    if (fd >= 0)
        pspDebugScreenPrintf("Device close 0x%08x\n", (unsigned)sceIoClose(fd));
    if (dispatched)
        pspDebugScreenPrintf("Use the PC reader to verify the trial.\n");
    else
        pspDebugScreenPrintf("No trial write was dispatched.\n");
}

static unsigned poll_buttons(void)
{
    SceCtrlData pad = {0};
    sceKernelDelayThread(20000);
    if (sceCtrlPeekBufferPositive(&pad, 1) > 0)
        return pad.Buttons;
    return 0;
}

static int confirm(unsigned mask)
{
    unsigned pressed;
    while (poll_buttons() & (mask | PSP_CTRL_CIRCLE))
        ;
    for (;;) {
        pressed = poll_buttons();
        if (pressed & PSP_CTRL_CIRCLE)
            return 0;
        if ((pressed & mask) == mask)
            return 1;
    }
}

#ifdef MSPART_TRIAL
static void show_confirmation(void)
{
    pspDebugScreenPrintf("\nControl sectors and PC backup match.\n");
    unsigned i;
    pspDebugScreenPrintf("Three primary partitions. MBR at LBA 0.\n");
    for (i = 0; i < TRIAL_PARTITIONS; ++i)
        pspDebugScreenPrintf("%u. Start %u, sectors %u\n", i + 1,
                             (unsigned)layout[i].start, (unsigned)layout[i].count);
    pspDebugScreenPrintf("The old exFAT volume will become inaccessible.\n"
                         "L + R + X: replace MBR    O: cancel\n");
}
#endif

int main(void)
{
    const char *error;
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenPrintf(TRIAL_TITLE "\n%s\n\n", MSWRITE_BUILD_ID);
    if (!load_reference()) {
        pspDebugScreenPrintf("Missing or invalid " TRIAL_FILE ".\n");
        goto done;
    }
    pspDebugScreenPrintf("Use only the backed-up 256 GB trial card.\n"
                         "Swap when the Memory Stick light is off.\n"
                         "X: check access    O: exit\n");
    if (!confirm(PSP_CTRL_CROSS))
        goto exit;
    error = preview();
    if (error) {
        pspDebugScreenPrintf("\n%s\n", error);
        goto done;
    }
#ifdef MSPART_TRIAL
    show_confirmation();
#else
    pspDebugScreenPrintf("\nControl sectors and PC backup match.\n"
                         "One sector will be overwritten at LBA %u.\n"
                         "Use a card you can afford to erase.\n"
                         "L + R + X: write    O: cancel\n", TRIAL_LBA);
#endif
    if (!confirm(CONFIRM_BUTTONS))
        goto exit;
    pspDebugScreenPrintf("\nWriting trial sector. Leave the card inserted.\n");
    run_write();
done:
    pspDebugScreenPrintf("\nO: exit\n");
    while (poll_buttons() & PSP_CTRL_CIRCLE)
        ;
    while (!(poll_buttons() & PSP_CTRL_CIRCLE))
        ;
exit:
    sctrlKernelExitVSH(NULL);
    return 0;
}
