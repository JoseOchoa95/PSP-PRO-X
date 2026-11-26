/* Standalone read-only partition switch trial. GPL-3.0-or-later. */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <psputils.h>
#include <string.h>
#include <systemctrl.h>

#include "fat32_probe.h"
#include "fat32_ram.h"
#include "native_fat_read.h"
#include "partition_view.h"
#include "stock_reader.h"
#include "write_trial.h"

PSP_MODULE_INFO("ProxMsSwitchTrial", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#ifdef MSFORMAT_TRIAL
#define TRIAL_FOLDER "MSFORMAT"
#define TRIAL_TITLE "PRO-X FAT32 Partition Format Trial"
#define CONFIRM_FORMAT (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_CROSS)
#else
#define TRIAL_FOLDER "MSSWITCH"
#define TRIAL_TITLE "PRO-X Memory Stick Partition Switch Trial"
#endif
#define REF_PATH "ms0:/PSP/GAME/" TRIAL_FOLDER "/MSPART.BIN"
#define RAW_USB_CAPACITY 0x02125006
#define RAW_MEDIA_ATTRIBUTES 0x02125803
#define DRIVER_NAME "pxpart"
#define TRIAL_PARTITIONS 3
#define SONY_TRIAL_DEVICE "pxpart3:"

static const char *const devices[TRIAL_PARTITIONS] = {"pxpart0:", "pxpart1:", "pxpart2:"};
static const uint32_t witnesses[TRIAL_PARTITIONS] = {2048, BINARY_CUTOFF, TRIAL_HIGH_LBA};
static struct WriteTrialReference reference;
static struct StockReader reader;
static struct NativeFatRead native_reader;
static struct PartitionView views[MBR_PARTITIONS];
static unsigned char sector[SECTOR_BYTES] __attribute__((aligned(64)));
static SceUID owner, raw = -1, active_fd = -1;
static int registered;
static unsigned native_partition = TRIAL_PARTITIONS;

static struct {
    const struct PartitionView *view;
    int64_t offset;
    unsigned sectors_read;
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
    unsigned index = arg->fs_num;
    if (index == TRIAL_PARTITIONS)
        index = native_partition;
    if (sceKernelGetThreadId() != owner || channel.view || flags != PSP_O_RDONLY ||
        index >= TRIAL_PARTITIONS || (name && *name && strcmp(name, "/")))
        return -1;
    channel.view = &views[index];
    channel.offset = 0;
    channel.sectors_read = 0;
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
        channel.sectors_read++;
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

static const char *check_reader(SceUID fd)
{
    struct MediaGeometry geometry;
    unsigned char attributes[MEDIA_ATTRIBUTE_BYTES] __attribute__((aligned(64)));
    uint32_t blocks = 0;
    if (fd < 0)
        return "Could not open the test card.";
    SceOff end = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (sceIoIoctl(fd, RAW_USB_CAPACITY, NULL, 0, &blocks, sizeof(blocks)) < 0 ||
        blocks != TRIAL_REPORTED_BLOCKS || end != (SceOff)blocks * SECTOR_BYTES ||
        sceIoIoctl(fd, RAW_MEDIA_ATTRIBUTES, NULL, 0, attributes, sizeof(attributes)) < 0 ||
        !decode_media_geometry(&geometry, attributes) || geometry.bytes != (uint64_t)end ||
        geometry.block_size != 4096 || geometry.unit_bytes != SECTOR_BYTES)
        return "Wrong card geometry or adapter.";
    return stock_reader_init(&reader, blocks);
}

static const char *open_trial(void)
{
    raw = sceIoOpen("msstor:", PSP_O_RDONLY, 0);
    const char *error = check_reader(raw);
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
    pspDebugScreenPrintf(TRIAL_TITLE "\n%s\n\n",
                         MSSWITCH_BUILD_ID);
}

static int read_view_sector(void *context, uint32_t lba, unsigned char *buffer)
{
    SceUID fd = *(const SceUID *)context;
    SceOff offset = (SceOff)lba * SECTOR_BYTES;
    return sceIoLseek(fd, offset, PSP_SEEK_SET) == offset &&
           sceIoRead(fd, buffer, SECTOR_BYTES) == SECTOR_BYTES;
}

static const char *fat_result(enum Fat32ProbeResult result)
{
    switch (result) {
    case FAT32_PROBE_MATCHED_FILE: return "PXPROBE.BIN read";
    case FAT32_PROBE_NO_VOLUME: return "no FAT32 boot sector";
    case FAT32_PROBE_NO_FILE: return "PXPROBE.BIN not found";
    case FAT32_PROBE_BAD_LAYOUT: return "layout rejected";
    case FAT32_PROBE_READ_ERROR: return "read failed";
    }
    return "unknown result";
}

static int matches_file_witness(unsigned char file[SECTOR_BYTES])
{
    unsigned char digest[DIGEST_BYTES];
    return !sceKernelUtilsSha1Digest(file, SECTOR_BYTES, digest) &&
           !memcmp(digest, reference.witnesses[3], sizeof(digest));
}

static void show_fat_trial(unsigned index)
{
    const struct PartitionView *view = &views[index];
    unsigned char file[SECTOR_BYTES];
    pspDebugScreenPrintf("FAT32 read trial   Partition %u/%u\n\n", index + 1, TRIAL_PARTITIONS);
    if (!matches_mbr()) {
        pspDebugScreenPrintf("The MBR does not match the PC reference.\n");
        return;
    }
    active_fd = sceIoOpen(devices[index], PSP_O_RDONLY, 0);
    if (active_fd < 0) {
        pspDebugScreenPrintf("Block device open failed: 0x%08x\n", (unsigned)active_fd);
        return;
    }
    enum Fat32ProbeResult physical = fat32_read_probe(read_view_sector, &active_fd, view->sectors, file);
    pspDebugScreenPrintf("On-card FAT32: %s\n", fat_result(physical));
    if (view->sectors == FAT32_TRIAL_SECTORS && view->start + FAT32_TRIAL_FILE_SECTOR == TRIAL_HIGH_LBA) {
        if (physical == FAT32_PROBE_MATCHED_FILE)
            pspDebugScreenPrintf("On-card file witness: %s\n", matches_file_witness(file) ? "MATCH" : "FAIL");

        struct Fat32Trial trial = {read_view_sector, &active_fd, 0};
        enum Fat32ProbeResult fixture = fat32_read_probe(fat32_trial_read, &trial, view->sectors, file);
        int match = fixture == FAT32_PROBE_MATCHED_FILE && trial.backing_reads == 1 &&
                    matches_file_witness(file);
        pspDebugScreenPrintf("\nFAT32 metadata in RAM: %s\n"
                             "File sector from card: LBA %u\n"
                             "Card data reads: %u\n"
                             "PC file witness: %s\n",
                             fat_result(fixture), (unsigned)TRIAL_HIGH_LBA, trial.backing_reads,
                             match ? "MATCH" : "FAIL");
    }
    int closed = sceIoClose(active_fd);
    if (closed >= 0)
        active_fd = -1;
    pspDebugScreenPrintf("\nDevice close 0x%08x\n", (unsigned)closed);
}

static int read_native_view(void *context, uint32_t lba, unsigned char *buffer)
{
    struct NativeFatRead *native = context;
    unsigned before = channel.sectors_read;
    int result = native_fat_read(native, lba);
    if (result < 0 || channel.sectors_read - before != 1 ||
        channel.offset != ((int64_t)lba + 1) * SECTOR_BYTES)
        return 0;
    memcpy(buffer, native->sector, SECTOR_BYTES);
    return 1;
}

static int check_native_partition(unsigned index)
{
    const struct PartitionView *view = &views[index];
    unsigned char sample[SECTOR_BYTES], digest[DIGEST_BYTES];
    native_partition = index;
    active_fd = sceIoOpen(SONY_TRIAL_DEVICE, PSP_O_RDONLY, 0);
    native_partition = TRIAL_PARTITIONS;
    if (active_fd < 0) {
        pspDebugScreenPrintf("P%u open failed 0x%08x\n", index + 1, (unsigned)active_fd);
        return 0;
    }
    int mount = native_fat_mount(&native_reader, SONY_TRIAL_DEVICE, active_fd);
    int match = 0, bounds = 0, unmount = -1, file_match = 1;
    if (!mount) {
        uint32_t relative = witnesses[index] - view->start;
        match = read_native_view(&native_reader, relative, sample) &&
                !sceKernelUtilsSha1Digest(sample, sizeof(sample), digest) &&
                !memcmp(digest, reference.witnesses[index + 1], sizeof(digest));
        unsigned before = channel.sectors_read;
        bounds = !read_native_view(&native_reader, view->sectors, sample) &&
                 channel.sectors_read == before;
        if (index == 2)
            file_match = fat32_read_probe(read_native_view, &native_reader, view->sectors, sample) ==
                         FAT32_PROBE_MATCHED_FILE && matches_file_witness(sample);
        unmount = native_fat_unmount(&native_reader);
    }
    int closed = sceIoClose(active_fd);
    if (closed >= 0)
        active_fd = -1;
    pspDebugScreenPrintf("P%u: witness %s, boundary %s\n"
                         "  mount %08x  unmount %08x  close %08x\n",
                         index + 1, match ? "MATCH" : "FAIL", bounds ? "PASS" : "FAIL",
                         (unsigned)mount, (unsigned)unmount, (unsigned)closed);
    if (index == 2)
        pspDebugScreenPrintf("  On-card FAT32 file: %s\n", file_match ? "MATCH" : "FAIL");
    return !mount && match && bounds && file_match && !unmount && closed >= 0;
}

static void show_native_trial(void)
{
    static const unsigned order[] = {0, 2, 0};
    pspDebugScreenPrintf("Sony FAT block switch (read only)\n"
                         "Private context, device %s\n\n", SONY_TRIAL_DEVICE);
    const char *error = native_fat_check(&native_reader);
    if (error) {
        pspDebugScreenPrintf("%s\n", error);
        return;
    }
    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        if (!matches_mbr() || !check_native_partition(order[i])) {
            pspDebugScreenPrintf("\nSony block switch: FAIL\n");
            return;
        }
    }
    pspDebugScreenPrintf("\nSony block switch: PASS\n");
}

static void show_native_format_trial(void)
{
    static struct Fat32Ram ram;
    const struct PartitionView *view = &views[2];
    unsigned char before[SECTOR_BYTES], after[SECTOR_BYTES];

    pspDebugScreenPrintf("Sony format dispatch (RAM only)\n"
                         "Partition 3: LBA %u through %u\n\n",
                         (unsigned)view->start, (unsigned)(view->start + view->sectors - 1));
    const char *error = native_fat_check(&native_reader);
    if (error || !matches_mbr()) {
        pspDebugScreenPrintf("%s\n", error ? error : "Trial card identity check failed.");
        return;
    }
    native_partition = 2;
    active_fd = sceIoOpen(SONY_TRIAL_DEVICE, PSP_O_RDONLY, 0);
    native_partition = TRIAL_PARTITIONS;
    if (active_fd < 0) {
        pspDebugScreenPrintf("Partition open failed 0x%08x\n", (unsigned)active_fd);
        return;
    }
    memset(&ram, 0, sizeof(ram));
    ram.start = view->start;
    ram.length = view->sectors;
    int mount = native_fat_mount(&native_reader, SONY_TRIAL_DEVICE, active_fd);
    int checked = !mount && read_native_view(&native_reader, 0, before) &&
                  fat32_read_probe(read_native_view, &native_reader, view->sectors, after) ==
                  FAT32_PROBE_MATCHED_FILE && matches_file_witness(after);
    int dispatch = checked ? native_fat_format_ram(&native_reader, &ram) : -1;
    int metadata = dispatch == 0 && fat32_ram_verify(&ram) &&
                   fat32_read_probe(fat32_ram_read, &ram, ram.length, after) == FAT32_PROBE_NO_FILE;
    int unchanged = checked && read_native_view(&native_reader, 0, after) &&
                    !memcmp(before, after, sizeof(before)) && matches_mbr();
    int unmount = !mount ? native_fat_unmount(&native_reader) : -1;
    int closed = sceIoClose(active_fd);
    if (closed >= 0)
        active_fd = -1;
    pspDebugScreenPrintf("Selected file witness: %s\n"
                         "Sony dispatch: 0x%08x\n"
                         "RAM writes: %u, FAT32 metadata: %s\n"
                         "Card boot sector and MBR: %s\n"
                         "Unmount: 0x%08x  close: 0x%08x\n\n"
                         "RAM format trial: %s\n",
                         checked ? "MATCH" : "FAIL", (unsigned)dispatch, (unsigned)ram.writes,
                         metadata ? "PASS" : "FAIL", unchanged ? "UNCHANGED" : "FAIL",
                         (unsigned)unmount, (unsigned)closed,
                         metadata && unchanged && !unmount && closed >= 0 ? "PASS" : "FAIL");
}

#ifdef MSFORMAT_TRIAL
static unsigned char confirmed_boot[SECTOR_BYTES];

static int volume_open(const char *path)
{
    SceUID fd = sceIoDopen(path);
    if (fd < 0)
        return 0;
    sceIoDclose(fd);
    return 1;
}

static int mounted_memory_stick(void)
{
    return volume_open("ms0:/") || volume_open("fatms0:/");
}

static const char *physical_precheck(void)
{
    if (views[2].start != TRIAL_FORMAT_START || views[2].sectors != TRIAL_FORMAT_SECTORS ||
        !matches_mbr())
        return "The exact test MBR and partition 3 are required.";
    if (mounted_memory_stick())
        return "A stock Memory Stick volume is mounted.";
    return native_fat_check(&native_reader);
}

static const char *open_physical_view(unsigned char boot[SECTOR_BYTES])
{
    const char *error = physical_precheck();
    if (error)
        return error;
    native_partition = 2;
    active_fd = sceIoOpen(SONY_TRIAL_DEVICE, PSP_O_RDONLY, 0);
    native_partition = TRIAL_PARTITIONS;
    if (active_fd < 0)
        return "Could not open the bounded volume view.";
    if (native_fat_mount(&native_reader, SONY_TRIAL_DEVICE, active_fd))
        return "The private Sony FAT context could not mount the view.";
    if (!read_native_view(&native_reader, 0, boot))
        return "Could not read the selected FAT32 boot sector.";
    if (!fat32_matches_extent(boot, views[2].start, views[2].sectors))
        return "The FAT32 boot sector does not describe all of partition 3.";
    unsigned char file[SECTOR_BYTES];
    if (fat32_read_probe(read_native_view, &native_reader, views[2].sectors, file) != FAT32_PROBE_MATCHED_FILE ||
        !matches_file_witness(file))
        return "The original partition 3 file did not match the PC witness.";
    if (!read_native_view(&native_reader, 0, file) || memcmp(boot, file, SECTOR_BYTES))
        return "The FAT32 boot sector changed during the card check.";
    return NULL;
}

static int close_physical_view(void)
{
    int unmount = native_reader.context.mounts ? native_fat_unmount(&native_reader) : 0;
    int closed = active_fd >= 0 ? sceIoClose(active_fd) : 0;
    if (closed >= 0)
        active_fd = -1;
    return !unmount && closed >= 0;
}

static int preview_physical_format(void)
{
    const char *error = open_physical_view(confirmed_boot);
    if (!error)
        error = stock_write_trial_check(&reader, NULL);
    if (!close_physical_view())
        error = "Could not close the preview.";
    if (error) {
        pspDebugScreenPrintf("%s\n", error);
        return 0;
    }
    pspDebugScreenPrintf("Format trial for the backed-up 256 GB card\n\n"
                         "Partition 3 only, LBA %u through %u\n"
                         "This replaces its FAT32 metadata and test file.\n"
                         "Result: an empty FAT32 volume.\n\n"
                         "L + R + X: format    O: cancel\n",
                         (unsigned)TRIAL_FORMAT_START, (unsigned)(TRIAL_FORMAT_START + TRIAL_FORMAT_SECTORS - 1));
    return 1;
}

static void run_physical_format(void)
{
    struct StockFormatTrial trial = {
        .reader = &reader, .reference = &reference, .confirmed_boot = confirmed_boot,
        .worker_result = -1,
    };
    if (sceIoClose(raw) < 0) {
        pspDebugScreenPrintf("Could not close the read-only handle.\nO: exit\n");
        return;
    }
    raw = sceIoOpen("msstor:", PSP_O_RDWR, 0);
    const char *error = check_reader(raw);
    unsigned char current_boot[SECTOR_BYTES];
    if (!error)
        error = open_physical_view(current_boot);
    if (!error && memcmp(current_boot, confirmed_boot, SECTOR_BYTES))
        error = "The FAT32 boot sector changed since the preview. No writes made.";
    int dispatch = -1;
    if (!error) {
        pspDebugScreenPrintf("Formatting partition 3. Keep the card inserted.\n"
                             "Writing and verifying metadata takes several minutes.\n");
        dispatch = native_fat_format_card(&native_reader, &trial);
        if (trial.error)
            error = trial.error;
    }
    int closed = close_physical_view() && close_trial();
    if (error)
        pspDebugScreenPrintf("%s\n", error);
    pspDebugScreenPrintf("\nSony dispatch: 0x%08x\n"
                         "Sector writes: %u, verified sectors: %u\n"
                         "Worker: 0x%08x  command: 0x%08x\n"
                         "Cleanup: %s\n"
                         "Physical format trial: %s\n\n"
                         "Use the direct PC reader for verification.\nO: exit\n",
                         (unsigned)dispatch, trial.writes, trial.verified,
                         (unsigned)trial.worker_result, (unsigned)trial.command_result,
                         closed ? "PASS" : "FAIL", !error && !dispatch && closed ? "PASS" : "FAIL");
}
#endif

static void show_actions(void)
{
    pspDebugScreenPrintf("\nLeft/Right: switch    Triangle: FAT32\n"
                         "Square: Sony block switch    Start: RAM format\n"
                         "O: exit\n");
#ifdef MSFORMAT_TRIAL
    pspDebugScreenPrintf("Select: preview physical format\n");
#endif
}

int main(void)
{
    SceCtrlData pad;
    unsigned previous = 0, selected = 0;
#ifdef MSFORMAT_TRIAL
    int armed = 0, attempted = 0;
#endif
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
        : "Missing or invalid MSPART.BIN in PSP/GAME/" TRIAL_FOLDER ".\n\nO: exit\n");
    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
#ifdef MSFORMAT_TRIAL
        if (armed) {
            if (pressed & PSP_CTRL_CIRCLE) {
                armed = 0;
                heading();
                pspDebugScreenPrintf("Format cancelled.\n");
                show_actions();
            } else if ((pressed & PSP_CTRL_CROSS) && (pad.Buttons & CONFIRM_FORMAT) == CONFIRM_FORMAT) {
                armed = 0;
                attempted = 1;
                heading();
                run_physical_format();
            }
            sceKernelDelayThread(20000);
            continue;
        }
        if (attempted && !(pressed & PSP_CTRL_CIRCLE)) {
            sceKernelDelayThread(20000);
            continue;
        }
        if (registered && active_fd < 0 && (pressed & PSP_CTRL_SELECT)) {
            heading();
            armed = preview_physical_format();
            if (!armed)
                show_actions();
            sceKernelDelayThread(20000);
            continue;
        }
#endif
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
                show_actions();
            }
        } else if (registered && active_fd < 0 && (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) {
            selected = (selected + ((pressed & PSP_CTRL_RIGHT) ? 1 : TRIAL_PARTITIONS - 1)) % TRIAL_PARTITIONS;
            heading();
            show_trial(selected);
            show_actions();
        } else if (registered && active_fd < 0 && (pressed & PSP_CTRL_TRIANGLE)) {
            heading();
            show_fat_trial(selected);
            show_actions();
        } else if (registered && active_fd < 0 && (pressed & PSP_CTRL_SQUARE)) {
            heading();
            show_native_trial();
            show_actions();
        } else if (registered && active_fd < 0 && (pressed & PSP_CTRL_START)) {
            heading();
            show_native_format_trial();
            show_actions();
        }
        sceKernelDelayThread(20000);
    }
}
