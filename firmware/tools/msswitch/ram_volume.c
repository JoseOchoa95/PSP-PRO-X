/* GPL-3.0-or-later */
#include <pspiofilemgr_kernel.h>
#include <pspthreadman.h>
#include <string.h>

#include "ram_volume.h"

#define DEVICE "pxdirs0:"
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

int ms_ram_volume_cleanup(void)
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

int ms_ram_volume_stage(struct Fat32Ram *output, uint32_t start, uint32_t length, uint32_t serial)
{
    if (fd >= 0 || registered || !sony_mkdir_ram_check() || !prepare_volume(start, length, serial) ||
        sceIoAddDrv(&driver) < 0)
        return 0;
    owner = sceKernelGetThreadId();
    registered = 1;
    fd = sceIoOpen(DEVICE, PSP_O_RDWR, 0);
    int ok = 0;
    if (fd >= 0) {
        struct SonyMkdirResult created = sony_mkdir_ram_run(DEVICE, fd, volume.length, 0);
        ok = !created.result && created.created == PSP_INITIAL_DIRECTORY_COUNT &&
             !created.unmount && !created.mutex_close && created.boundary_rejected &&
             disk.rejected_bounds == 2 && fat32_ram_add_indicators(&volume);
    }
    if (!ms_ram_volume_cleanup() || !ok)
        return 0;
    *output = volume;
    return 1;
}

#ifdef MSDIRS_TRIAL
enum MsRamTrialStatus ms_ram_volume_test(uint32_t start, uint32_t length, uint32_t serial,
                                          struct Fat32Ram *output, struct MsRamTrial *trial)
{
    if (fd >= 0 || registered || !prepare_volume(start, length, serial) || sceIoAddDrv(&driver) < 0)
        return MS_RAM_TRIAL_PREPARE;
    owner = sceKernelGetThreadId();
    registered = 1;
    fd = sceIoOpen(DEVICE, PSP_O_RDWR, 0);
    if (fd < 0) {
        trial->open_error = fd;
        ms_ram_volume_cleanup();
        return MS_RAM_TRIAL_OPEN;
    }

    struct SonyMkdirResult created = sony_mkdir_ram_run(DEVICE, fd, volume.length, 0);
    unsigned writes = disk.writes;
    int bounds = created.boundary_rejected && disk.rejected_bounds == 2;
    int created_all = !created.result && created.created == PSP_INITIAL_DIRECTORY_COUNT;
    int tree = created_all && writes > 0 && fat32_ram_add_indicators(&volume) &&
               fat32_ram_finish_directories(&volume);
    if (tree && output)
        *output = volume;
    int denied = 0;
    int try_denied = tree && bounds && !created.unmount && !created.mutex_close;
    if (try_denied && prepare_volume(start, length, serial)) {
        disk.deny = 1;
        struct SonyMkdirResult failure = sony_mkdir_ram_run(DEVICE, fd, volume.length, 1);
        disk.deny = 0;
        denied = failure.result < 0 && !failure.created && disk.denied_writes &&
                 !disk.writes && !failure.unmount && !failure.mutex_close && fat32_ram_verify(&volume);
    }
    int clean = ms_ram_volume_cleanup();
    *trial = (struct MsRamTrial){created, writes, created_all, tree, bounds, try_denied, denied, clean, 0};
    return MS_RAM_TRIAL_RAN;
}
#endif
