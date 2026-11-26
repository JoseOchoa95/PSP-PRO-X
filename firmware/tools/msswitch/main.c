/* Standalone read-only partition switch trial. GPL-3.0-or-later. */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <psputils.h>
#include <string.h>
#include <systemctrl.h>

#include "partition_view.h"
#include "stock_reader.h"
#include "write_trial.h"

PSP_MODULE_INFO("ProxMsSwitchTrial", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#define REF_PATH "ms0:/PSP/GAME/MSSWITCH/MSPART.BIN"
#define RAW_USB_CAPACITY 0x02125006
#define RAW_MEDIA_ATTRIBUTES 0x02125803
#define DRIVER_NAME "pxpart"
#define TRIAL_PARTITIONS 3

static const char *const devices[TRIAL_PARTITIONS] = {"pxpart0:", "pxpart1:", "pxpart2:"};
static const uint32_t witnesses[TRIAL_PARTITIONS] = {2048, BINARY_CUTOFF, TRIAL_HIGH_LBA};
static struct WriteTrialReference reference;
static struct StockReader reader;
static struct PartitionView views[MBR_PARTITIONS];
static unsigned char sector[SECTOR_BYTES] __attribute__((aligned(64)));
static SceUID owner, raw = -1, active_fd = -1;
static int registered;

static struct {
    const struct PartitionView *view;
    int64_t offset;
} channel;

static int read_raw(uint32_t lba)
{
    int result = -1;
    return !stock_read_sector(&reader, lba, sector, &result, NULL) && result == SECTOR_BYTES;
}

static int load_reference(void)
{
    unsigned char extra;
    SceUID fd = sceIoOpen(REF_PATH, PSP_O_RDONLY, 0);
    if (fd < 0)
        return 0;
    int size = sceIoRead(fd, &reference, sizeof(reference));
    int tail = sceIoRead(fd, &extra, 1);
    int closed = sceIoClose(fd);
    return size == (int)sizeof(reference) && tail == 0 && closed >= 0 &&
           !memcmp(reference.magic, "MSPREF01", 8);
}

static int init_driver(PspIoDrvArg *arg)
{
    (void)arg;
    return 0;
}

static int open_view(PspIoDrvFileArg *arg, char *name, int flags, SceMode mode)
{
    (void)mode;
    if (sceKernelGetThreadId() != owner || channel.view || flags != PSP_O_RDONLY ||
        (unsigned)arg->fs_num >= TRIAL_PARTITIONS || (name && *name && strcmp(name, "/")))
        return -1;
    channel.view = &views[arg->fs_num];
    channel.offset = 0;
    arg->arg = &channel;
    return 0;
}

static int close_view(PspIoDrvFileArg *arg)
{
    if (arg->arg != &channel || !channel.view)
        return -1;
    channel.view = NULL;
    arg->arg = NULL;
    return 0;
}

static int read_view(PspIoDrvFileArg *arg, char *buffer, int size)
{
    if (arg->arg != &channel || !channel.view || size < 0 || size % SECTOR_BYTES)
        return -1;
    int read = 0;
    while (read < size) {
        uint32_t lba;
        if (!view_sector(channel.view, channel.offset / SECTOR_BYTES, &lba))
            break;
        if (!read_raw(lba))
            return read ? read : -1;
        memcpy(buffer + read, sector, SECTOR_BYTES);
        read += SECTOR_BYTES;
        channel.offset += SECTOR_BYTES;
    }
    return read;
}

static SceOff seek_view(PspIoDrvFileArg *arg, SceOff offset, int whence)
{
    if (arg->arg != &channel || !channel.view)
        return -1;
    int64_t position = view_seek(channel.view, channel.offset, offset, whence);
    if (position >= 0)
        channel.offset = position;
    return position;
}

static PspIoDrvFuncs functions = {
    .IoInit = init_driver,
    .IoExit = init_driver,
    .IoOpen = open_view,
    .IoClose = close_view,
    .IoRead = read_view,
    .IoLseek = seek_view,
};

static PspIoDrv driver = {
    .name = DRIVER_NAME,
    .dev_type = 4,
    .unk2 = SECTOR_BYTES,
    .name2 = "PRO-X partition trial",
    .funcs = &functions,
};

static int matches_mbr(void)
{
    return read_raw(0) && !memcmp(sector, reference.marker, SECTOR_BYTES);
}

static const char *open_trial(void)
{
    struct MediaGeometry geometry;
    unsigned char attributes[MEDIA_ATTRIBUTE_BYTES] __attribute__((aligned(64)));
    uint32_t blocks = 0;
    raw = sceIoOpen("msstor:", PSP_O_RDONLY, 0);
    if (raw < 0)
        return "Could not open the test card.";
    SceOff end = sceIoLseek(raw, 0, PSP_SEEK_END);
    if (sceIoIoctl(raw, RAW_USB_CAPACITY, NULL, 0, &blocks, sizeof(blocks)) < 0 ||
        blocks != TRIAL_REPORTED_BLOCKS || end != (SceOff)blocks * SECTOR_BYTES ||
        sceIoIoctl(raw, RAW_MEDIA_ATTRIBUTES, NULL, 0, attributes, sizeof(attributes)) < 0 ||
        !decode_media_geometry(&geometry, attributes) || geometry.bytes != (uint64_t)end ||
        geometry.block_size != 4096 || geometry.unit_bytes != SECTOR_BYTES)
        return "Wrong card geometry or adapter.";
    const char *error = stock_reader_init(&reader, blocks);
    if (error)
        return error;
    if (!matches_mbr() || read_primary_views(sector, TRIAL_SECTORS, views) != TRIAL_PARTITIONS)
        return "The MBR does not match the PC reference.";
    int result = sceIoAddDrv(&driver);
    if (result < 0) {
        pspDebugScreenPrintf("sceIoAddDrv 0x%08x\n", (unsigned)result);
        return "Could not register the trial driver.";
    }
    registered = 1;
    return NULL;
}

static int close_trial(void)
{
    if (active_fd >= 0) {
        if (sceIoClose(active_fd) < 0)
            return 0;
        active_fd = -1;
    }
    if (registered) {
        if (sceIoDelDrv(DRIVER_NAME) < 0)
            return 0;
        registered = 0;
    }
    if (raw >= 0) {
        if (sceIoClose(raw) < 0)
            return 0;
        raw = -1;
    }
    return 1;
}

static void show_trial(unsigned index)
{
    const struct PartitionView *view = &views[index];
    uint32_t witness = witnesses[index];
    unsigned char sample[SECTOR_BYTES], digest[DIGEST_BYTES];
    SceOff end, after_end, before_start;
    int read = -1, eof = -1, matches = 0;
    SceOff expected_end = (SceOff)view->sectors * SECTOR_BYTES;

    pspDebugScreenPrintf("Partition %u/%u   Device %s\n"
                         "Start %u  Last %u\n",
                         index + 1, TRIAL_PARTITIONS, devices[index], (unsigned)view->start,
                         (unsigned)(view->start + view->sectors - 1));
    if (active_fd >= 0 || !matches_mbr() || witness < view->start ||
        witness - view->start >= view->sectors) {
        pspDebugScreenPrintf("Trial stopped. Check the card and adapter.\n");
        return;
    }
    SceUID write_fd = sceIoOpen(devices[index], PSP_O_WRONLY, 0);
    if (write_fd >= 0) {
        active_fd = write_fd;
        pspDebugScreenPrintf("FAIL: unexpected writable handle. Press O.\n");
        return;
    }
    active_fd = sceIoOpen(devices[index], PSP_O_RDONLY, 0);
    if (active_fd < 0) {
        pspDebugScreenPrintf("FAIL: read open 0x%08x\n", (unsigned)active_fd);
        return;
    }
    SceOff offset = (SceOff)(witness - view->start) * SECTOR_BYTES;
    if (sceIoLseek(active_fd, offset, PSP_SEEK_SET) == offset) {
        read = sceIoRead(active_fd, sample, sizeof(sample));
        matches = read == SECTOR_BYTES && !sceKernelUtilsSha1Digest(sample, sizeof(sample), digest) &&
                  !memcmp(digest, reference.witnesses[index + 1], sizeof(digest));
    }
    end = sceIoLseek(active_fd, 0, PSP_SEEK_END);
    if (end == expected_end)
        eof = sceIoRead(active_fd, sample, sizeof(sample));
    after_end = sceIoLseek(active_fd, SECTOR_BYTES, PSP_SEEK_END);
    before_start = sceIoLseek(active_fd, -SECTOR_BYTES, PSP_SEEK_SET);
    int closed = sceIoClose(active_fd);
    if (closed >= 0)
        active_fd = -1;
    int bounds = end == expected_end && eof == 0 && after_end < 0 && before_start < 0;
    pspDebugScreenPrintf("Witness LBA %u (relative %u), read %d\n"
                         "PC witness: %s\n"
                         "Length %lld bytes, EOF %d\n"
                         "Out-of-range seeks: %s\n"
                         "Write open rejected, close 0x%08x\n\n"
                         "Partition test: %s\n",
                         (unsigned)witness, (unsigned)(witness - view->start), read,
                         matches ? "MATCH" : "FAIL", (long long)end, eof,
                         after_end < 0 && before_start < 0 ? "rejected" : "FAIL",
                         (unsigned)closed, matches && bounds && closed >= 0 ? "PASS" : "FAIL");
}

static void heading(void)
{
    pspDebugScreenClear();
    pspDebugScreenPrintf("PRO-X Memory Stick Partition Switch Trial\n%s\n\n",
                         MSSWITCH_BUILD_ID);
}

int main(void)
{
    SceCtrlData pad;
    unsigned previous = 0, selected = 0;
    owner = sceKernelGetThreadId();
    pspDebugScreenInit();
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    heading();
    int has_reference = load_reference();
    pspDebugScreenPrintf(has_reference
        ? "PC reference loaded.\n"
          "Insert the backed-up 256 GB trial card.\n"
          "Use its original Memory Stick adapter.\n"
          "Swap only when the Memory Stick light is off.\n\n"
          "X: check card    O: exit\n"
        : "Missing or invalid MSPART.BIN in PSP/GAME/MSSWITCH.\n\nO: exit\n");
    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) {
            heading();
            if (close_trial()) {
                int result = sctrlKernelExitVSH(NULL);
                if (result >= 0)
                    return 0;
                pspDebugScreenPrintf("Exit returned 0x%08x. Press O to retry.\n", (unsigned)result);
            } else {
                pspDebugScreenPrintf("Cleanup incomplete. Press O to retry or restart the PSP.\n");
            }
        } else if ((pressed & PSP_CTRL_CROSS) && has_reference && raw < 0) {
            heading();
            const char *error = open_trial();
            if (error) {
                pspDebugScreenPrintf("%s\n\n", error);
                pspDebugScreenPrintf(close_trial() ? "X: retry    O: exit\n" : "Press O to retry cleanup.\n");
            } else {
                show_trial(selected);
                pspDebugScreenPrintf("\nLeft/Right: switch    O: exit\n");
            }
        } else if (registered && active_fd < 0 && (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) {
            selected = (selected + ((pressed & PSP_CTRL_RIGHT) ? 1 : TRIAL_PARTITIONS - 1)) % TRIAL_PARTITIONS;
            heading();
            show_trial(selected);
            pspDebugScreenPrintf("\nLeft/Right: switch    O: exit\n");
        }
        sceKernelDelayThread(20000);
    }
}
