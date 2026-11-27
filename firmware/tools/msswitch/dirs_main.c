/* GPL-3.0-or-later */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <string.h>
#include <systemctrl.h>

#include "fat32_dirs.h"
#include "sony_mkdir_ram.h"
#ifdef MSDIRS_CARD_TRIAL
#include "dirs_card.h"
static struct Fat32Ram staged_volume;
#endif

PSP_MODULE_INFO("ProxRamDirectoryTrial", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#define DEVICE "pxdirs0:"
enum { PARTITION_START = 461375488u, PARTITION_SECTORS = 39319552u };
static const int io_error = (int)0x80010005;
static struct Fat32Ram volume;
static SceUID owner;
static SceUID fd = -1;
static int registered;
static struct {
    uint32_t position;
    unsigned writes, rejected_bounds, denied_writes;
    int opened, deny;
} disk;

static int driver_lifecycle(PspIoDrvArg *arg)
{
    (void)arg;
    return 0;
}

static int driver_open(PspIoDrvFileArg *arg, char *name, int flags, SceMode mode)
{
    (void)mode;
    if (sceKernelGetThreadId() != owner || disk.opened || arg->fs_num ||
        flags != PSP_O_RDWR || (name && *name && strcmp(name, "/")))
        return io_error;
    disk.opened = 1;
    arg->arg = &disk;
    return 0;
}

static int driver_close(PspIoDrvFileArg *arg)
{
    if (arg->arg != &disk)
        return io_error;
    disk.opened = 0;
    arg->arg = NULL;
    return 0;
}

static int valid_transfer(PspIoDrvFileArg *arg, int bytes)
{
    if (arg->arg != &disk || sceKernelGetThreadId() != owner ||
        bytes <= 0 || bytes % FAT32_SECTOR_BYTES)
        return 0;
    if ((unsigned)bytes / FAT32_SECTOR_BYTES > volume.length - disk.position) {
        ++disk.rejected_bounds;
        return 0;
    }
    return 1;
}

static int driver_read(PspIoDrvFileArg *arg, char *buffer, int bytes)
{
    if (!valid_transfer(arg, bytes))
        return io_error;
    for (int offset = 0; offset < bytes; offset += FAT32_SECTOR_BYTES) {
        if (!fat32_ram_read(&volume, disk.position, (unsigned char *)buffer + offset))
            return io_error;
        ++disk.position;
    }
    return bytes;
}

static int driver_write(PspIoDrvFileArg *arg, const char *buffer, int bytes)
{
    if (!valid_transfer(arg, bytes))
        return io_error;
    if (disk.deny) {
        ++disk.denied_writes;
        return io_error;
    }
    for (int offset = 0; offset < bytes; offset += FAT32_SECTOR_BYTES) {
        if (!fat32_ram_write(&volume, disk.position, (const unsigned char *)buffer + offset))
            return offset ? offset : io_error;
        ++disk.position;
    }
    ++disk.writes;
    return bytes;
}

static SceOff driver_seek(PspIoDrvFileArg *arg, SceOff offset, int whence)
{
    if (arg->arg != &disk || sceKernelGetThreadId() != owner || whence != PSP_SEEK_SET ||
        offset < 0 || offset % FAT32_SECTOR_BYTES)
        return io_error;
    if (offset > (SceOff)volume.length * FAT32_SECTOR_BYTES) {
        ++disk.rejected_bounds;
        return io_error;
    }
    disk.position = offset / FAT32_SECTOR_BYTES;
    return offset;
}

static int driver_ioctl(PspIoDrvFileArg *arg, unsigned command, void *input, int input_bytes,
                        void *output, int output_bytes)
{
    if (arg->arg != &disk || sceKernelGetThreadId() != owner || input || input_bytes || !output)
        return io_error;
    if (command == 0x02125803 && output_bytes == 96) {
        memset(output, 0, output_bytes);
        return 0;
    }
    if (command == 0x02125008 && output_bytes == 4) {
        const uint32_t present = 1;
        memcpy(output, &present, sizeof(present));
        return 0;
    }
    return io_error;
}

static PspIoDrvFuncs functions = {
    .IoInit = driver_lifecycle, .IoExit = driver_lifecycle,
    .IoOpen = driver_open, .IoClose = driver_close,
    .IoRead = driver_read, .IoWrite = driver_write, .IoLseek = driver_seek, .IoIoctl = driver_ioctl,
};

static PspIoDrv driver = {
    .name = "pxdirs", .dev_type = 4, .unk2 = FAT32_SECTOR_BYTES,
    .name2 = "PRO-X isolated RAM trial", .funcs = &functions,
};

static int prepare_volume(uint32_t start, uint32_t length, uint32_t serial)
{
    memset(&volume, 0, sizeof(volume));
    volume.start = start;
    volume.length = length;
    disk.position = disk.writes = disk.denied_writes = disk.rejected_bounds = 0;
    return fat32_initialize(start, length, serial, fat32_ram_write, &volume) ==
           FAT32_INIT_OK && fat32_ram_verify(&volume);
}

static int cleanup(void)
{
    if (fd >= 0) {
        if (sceIoClose(fd) < 0)
            return 0;
        fd = -1;
    }
    if (registered) {
        if (sceIoDelDrv("pxdirs") < 0)
            return 0;
        registered = 0;
    }
    return 1;
}

#ifdef MSMULTI_CARD_TRIAL
int dirs_card_stage(struct Fat32Ram *output, uint32_t start, uint32_t length, uint32_t serial)
{
    if (!sony_mkdir_ram_check() || !prepare_volume(start, length, serial) || sceIoAddDrv(&driver) < 0)
        return 0;
    registered = 1;
    fd = sceIoOpen(DEVICE, PSP_O_RDWR, 0);
    int ok = 0;
    if (fd >= 0) {
        struct SonyMkdirResult created = sony_mkdir_ram_run(DEVICE, fd, volume.length, 0);
        ok = !created.result && created.created == PSP_INITIAL_DIRECTORY_COUNT &&
             !created.unmount && !created.mutex_close && created.boundary_rejected &&
             disk.rejected_bounds == 2 && fat32_ram_add_indicators(&volume);
    }
    if (!cleanup() || !ok)
        return 0;
    *output = volume;
    return 1;
}
#endif

static int run_trial(void)
{
    if (!sony_mkdir_ram_check()) {
        pspDebugScreenPrintf("Sony 6.61 FAT module profile: REJECTED\n");
        return 0;
    }
    pspDebugScreenPrintf("Sony 6.61 FAT module profile: MATCH\n");
    if (!prepare_volume(PARTITION_START, PARTITION_SECTORS, 0x50585031) || sceIoAddDrv(&driver) < 0) {
        pspDebugScreenPrintf("Could not prepare the private RAM device.\n");
        return 0;
    }
    registered = 1;
    fd = sceIoOpen(DEVICE, PSP_O_RDWR, 0);
    if (fd < 0) {
        pspDebugScreenPrintf("RAM device open failed: 0x%08x\n", (unsigned)fd);
        cleanup();
        return 0;
    }

    struct SonyMkdirResult created = sony_mkdir_ram_run(DEVICE, fd, volume.length, 0);
    unsigned writes = disk.writes;
    int bounds = created.boundary_rejected && disk.rejected_bounds == 2;
    int created_all = !created.result && created.created == PSP_INITIAL_DIRECTORY_COUNT;
    int tree = created_all && writes > 0 && fat32_ram_add_indicators(&volume) &&
               fat32_ram_finish_directories(&volume);
#ifdef MSDIRS_CARD_TRIAL
    if (tree)
        staged_volume = volume;
#endif
    int denied = 0;
    int try_denied = tree && bounds && !created.unmount && !created.mutex_close;
    if (try_denied && prepare_volume(PARTITION_START, PARTITION_SECTORS, 0x50585031)) {
        disk.deny = 1;
        struct SonyMkdirResult failure = sony_mkdir_ram_run(DEVICE, fd, volume.length, 1);
        disk.deny = 0;
        denied = failure.result < 0 && !failure.created && disk.denied_writes &&
                 !disk.writes && !failure.unmount && !failure.mutex_close && fat32_ram_verify(&volume);
    }
    int clean = cleanup();
    int success = tree && bounds && denied && clean;
    pspDebugScreenPrintf("Sony mkdir: 0x%08x\nDirectories: %u/%u   RAM write calls: %u\n"
                         "Tree, two indicator files and FAT32 summary: %s\nPartition boundary: %s\n"
                         "Denied write propagation: %s\n"
                         "Unmount: 0x%08x  mutex close: 0x%08x\n"
                         "RAM device cleanup: %s\n\nRAM directory trial: %s\n",
                         (unsigned)created.result, created.created, PSP_INITIAL_DIRECTORY_COUNT, writes,
                         !created_all ? "NOT RUN" : tree ? "PASS" : "FAIL",
                         !created_all ? "NOT RUN" : bounds ? "PASS" : "FAIL",
                         !try_denied ? "NOT RUN" : denied ? "PASS" : "FAIL",
                         (unsigned)created.unmount, (unsigned)created.mutex_close,
                         clean ? "PASS" : "FAIL", success ? "PASS" : "FAIL");
    return success;
}

static void heading(void)
{
    pspDebugScreenClear();
#if defined(MSWRITE_PATH_TRIAL)
    pspDebugScreenPrintf("PRO-X Memory Stick Write Path Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#elif defined(MSAUTO_FORMAT_TRIAL)
    pspDebugScreenPrintf("PRO-X Memory Stick Auto Format\n%s\n\n", MSSWITCH_BUILD_ID);
#elif defined(MSMULTI_CARD_TRIAL)
    pspDebugScreenPrintf("PRO-X Memory Stick Whole Card Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#elif defined(MSDIRS_CARD_TRIAL)
    pspDebugScreenPrintf("PRO-X FAT32 Physical Directory Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#else
    pspDebugScreenPrintf("PRO-X FAT32 RAM Directory Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#endif
}

int main(void)
{
    owner = sceKernelGetThreadId();
    pspDebugScreenInit();
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    heading();
#ifdef MSDIRS_CARD_TRIAL
    enum { PREPARE, CHECK_CARD, CONFIRM, DONE };
#ifdef MSWRITE_PATH_TRIAL
    int stage = CHECK_CARD;
    pspDebugScreenPrintf("Wait for the Memory Stick light to go out.\n"
                         "Insert the backed-up 512 GB trial card.\n\nX: check card    O: exit\n");
#elif defined(MSAUTO_FORMAT_TRIAL)
    int stage = PREPARE;
    pspDebugScreenPrintf("X: check the formatter in RAM\nO: exit\n");
#else
    int stage = dirs_card_load_reference() ? PREPARE : DONE;
    pspDebugScreenPrintf(stage == PREPARE ? "X: prepare the initial filesystem in RAM\nO: exit\n" :
                         "Missing or invalid MSPART.BIN beside EBOOT.PBP.\nO: exit\n");
#endif
#else
    pspDebugScreenPrintf("Sony mkdir on a private FAT32 volume in RAM.\n"
                         "Leave the launch Memory Stick inserted.\n\nX: run trial    O: exit\n");
    int ran = 0;
#endif
    unsigned previous = 0;
    for (;;) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) {
            if (cleanup()
#ifdef MSDIRS_CARD_TRIAL
                && dirs_card_cleanup()
#endif
            ) {
                int result = sctrlKernelExitVSH(NULL);
                if (result >= 0)
                    return 0;
                pspDebugScreenPrintf("Exit failed: 0x%08x\n", (unsigned)result);
            } else {
                pspDebugScreenPrintf("RAM device cleanup incomplete. Press O to retry.\n");
            }
        }
#ifdef MSDIRS_CARD_TRIAL
#if defined(MSMULTI_CARD_TRIAL) && !defined(MSAUTO_FORMAT_TRIAL)
        else if (stage == CHECK_CARD && (pressed & PSP_CTRL_SQUARE)) {
            stage = DONE;
            heading();
            dirs_card_test_mount();
        }
        else if (stage == CHECK_CARD && (pressed & PSP_CTRL_START)) {
            heading();
            if (dirs_card_preview_repeat())
                stage = CONFIRM;
        }
#endif
        else if (pressed & PSP_CTRL_CROSS) {
            if (stage == PREPARE) {
                heading();
                stage = run_trial() ? CHECK_CARD : DONE;
                if (stage == CHECK_CARD)
                    pspDebugScreenPrintf("\nWhen the Memory Stick light is off, insert the\n"
#ifdef MSAUTO_FORMAT_TRIAL
                                         "backed-up microSD with its Memory Stick adapter.\n"
#else
                                         "backed-up 256 GB card with its original adapter.\n"
#endif
                                         "X: check card    O: exit\n"
#if defined(MSMULTI_CARD_TRIAL) && !defined(MSAUTO_FORMAT_TRIAL)
                                         "Square: test mount   Start: repeat format\n"
#endif
                    );
                else
                    pspDebugScreenPrintf("\nO: exit\n");
            } else if (stage == CHECK_CARD) {
                heading();
                if (dirs_card_preview())
                    stage = CONFIRM;
            } else if (stage == CONFIRM &&
                       (pad.Buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) ==
                           (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
                stage = DONE;
                heading();
                dirs_card_write(&staged_volume);
            }
        }
#else
        else if ((pressed & PSP_CTRL_CROSS) && !ran) {
            ran = 1;
            heading();
            run_trial();
            pspDebugScreenPrintf("\nO: exit\n");
        }
#endif
        sceKernelDelayThread(20000);
    }
}
