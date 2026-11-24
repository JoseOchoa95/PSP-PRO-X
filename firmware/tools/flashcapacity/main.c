/* Standalone flash0 capacity probe for 6.61. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <stdio.h>
#include "systemctrl.h"
#include "prox_flash_capacity.h"

/* Match the USB driver's kernel access for its read-only geometry request. */
PSP_MODULE_INFO("ProxFlashCapacity", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);
PSP_MAIN_THREAD_STACK_SIZE_KB(64);

typedef struct {
    SceUID fd;
    SceOff seek_result;
    int read_result;
} Reader;

static int read_metadata(void *context, uint32_t offset, void *buffer, unsigned int bytes)
{
    Reader *reader = context;

    reader->seek_result = sceIoLseek(reader->fd, (SceOff)offset, PSP_SEEK_SET);
    if (reader->seek_result != (SceOff)offset)
        return -1;
    reader->read_result = sceIoRead(reader->fd, buffer, bytes);
    return reader->read_result == (int)bytes ? 0 : -1;
}

static uint32_t flash_sector_count(const unsigned char *field)
{
    return (uint32_t)field[0] | (uint32_t)field[1] << 8 |
           (uint32_t)field[2] << 16 | (uint32_t)field[3] << 24;
}

static int save_report(const char *text, unsigned int size, char path[72])
{
    for (unsigned int number = 1; number <= 999; ++number) {
        snprintf(path, 72, "ms0:/PSP/GAME/FCAPDIAG/report-%03u.txt", number);
        SceUID report = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_EXCL, 0600);
        if (report == (SceUID)0x80010011)
            continue;
        if (report < 0)
            return -1;
        int count = sceIoWrite(report, text, size);
        int closed = sceIoClose(report);
        return count == (int)size && closed >= 0 ? 0 : -1;
    }
    return -1;
}

int main(void)
{
    char text[640], path[72];
    Reader reader = { .fd = -1 };
    ProxFlashCapacity capacity = {0};
    unsigned int firmware = sceKernelDevkitVersion();
    uint32_t sectors = 0, volume_bytes = 0;
    int geometry_result = -1, boot_read = -1;
    int status = PROX_FLASH_CAPACITY_GEOMETRY, close_result = 0;
    SceCtrlData pad;

    pspDebugScreenInit();
    pspDebugScreenPrintf("PRO-X Flash0 Capacity (read only)\nReading allocation metadata...\n");
    if (firmware == 0x06060110u) {
        reader.fd = sceIoOpen("lflash0:0,0", PSP_O_RDONLY, 0);
        if (reader.fd >= 0) {
            /* usbdevice uses this same read-only geometry request on lflash.
             * The sector count starts at offset 0x14 in the fixed reply. */
            unsigned char geometry[0x44] __attribute__((aligned(64))) = {0};
            unsigned char boot[PROX_FLASH_SECTOR_BYTES] __attribute__((aligned(64)));
            boot_read = sceIoRead(reader.fd, boot, sizeof(boot));
            geometry_result = sceIoIoctl(reader.fd, 0x0003d001u, NULL, 0,
                                          geometry, sizeof(geometry));
            if (geometry_result < 0) {
                status = PROX_FLASH_CAPACITY_READ;
            } else {
                sectors = flash_sector_count(geometry + 0x14);
                if (sectors && sectors <= PROX_FLASH_MAX_VOLUME_BYTES / PROX_FLASH_SECTOR_BYTES) {
                    volume_bytes = sectors * PROX_FLASH_SECTOR_BYTES;
                    status = prox_flash_capacity(read_metadata, &reader, volume_bytes, &capacity);
                }
            }
            close_result = sceIoClose(reader.fd);
            if (close_result < 0)
                status = PROX_FLASH_CAPACITY_READ;
        } else {
            status = PROX_FLASH_CAPACITY_READ;
        }
    }

    int count = snprintf(text, sizeof(text),
        "PRO-X Phase 3-B flash0 capacity build %s\n"
        "firmware=0x%08x result=%d open=0x%08x geometry=0x%08x boot_read=%d\n"
        "raw_sectors=%u volume_bytes=%u last_seek=%lld last_read=%d close=0x%08x\n"
        "FAT=%u cluster_bytes=%u total_bytes=%u free_bytes=%u\n",
        PROX_CAPACITY_BUILD, firmware, status, (unsigned int)reader.fd, (unsigned int)geometry_result, boot_read,
        sectors, volume_bytes, (long long)reader.seek_result, reader.read_result, (unsigned int)close_result,
        capacity.fat_bits, capacity.cluster_bytes, capacity.total_bytes, capacity.free_bytes);
    if (count > 0 && (unsigned int)count < sizeof(text)) {
        pspDebugScreenPrintf("%s", text);
        if (save_report(text, (unsigned int)count, path) == 0)
            pspDebugScreenPrintf("Saved %s\n", path);
    }
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenPrintf("Press X to return to XMB, or wait 30 seconds.\n");
    unsigned int start = sceKernelGetSystemTimeLow();
    do {
        int buttons = sceCtrlPeekBufferPositive(&pad, 1);
        if (buttons < 0 || (buttons > 0 && (pad.Buttons & PSP_CTRL_CROSS)))
            break;
        sceKernelDelayThread(20000);
    } while ((unsigned int)(sceKernelGetSystemTimeLow() - start) < 30000000u);
    /* All file descriptors are closed. Like WPA2DIAG, return via the
     * existing PRO kernel export after running in a kernel thread. */
    int exit_result = sctrlKernelExitVSH(NULL);
    pspDebugScreenPrintf("Return to XMB failed: 0x%08x\n", (unsigned int)exit_result);
    return 0;
}
