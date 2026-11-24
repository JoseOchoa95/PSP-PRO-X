/* PSP adapter for the read-only flash0 FAT capacity parser. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include "prox_flash_capacity.h"

static int read_metadata(void *context, uint32_t offset, void *buffer, unsigned int bytes)
{
    SceUID fd = *(const SceUID *)context;
    if (sceIoLseek(fd, offset, PSP_SEEK_SET) != (SceOff)offset)
        return -1;
    return sceIoRead(fd, buffer, bytes) == (int)bytes ? 0 : -1;
}

int prox_flash0_capacity(ProxFlashCapacity *out)
{
    unsigned char geometry[0x44] __attribute__((aligned(64))) = {0};
    unsigned char boot[PROX_FLASH_SECTOR_BYTES] __attribute__((aligned(64)));
    int result = PROX_FLASH_CAPACITY_READ;

    if (!out || sceKernelDevkitVersion() != 0x06060110u)
        return PROX_FLASH_CAPACITY_GEOMETRY;
    SceUID fd = sceIoOpen("lflash0:0,0", PSP_O_RDONLY, 0);
    if (fd < 0)
        return result;
    if (sceIoRead(fd, boot, sizeof(boot)) == (int)sizeof(boot) &&
        sceIoIoctl(fd, 0x0003d001u, NULL, 0, geometry, sizeof(geometry)) >= 0) {
        const unsigned char *field = geometry + 0x14;
        uint32_t sectors = (uint32_t)field[0] | (uint32_t)field[1] << 8 |
                           (uint32_t)field[2] << 16 | (uint32_t)field[3] << 24;
        result = PROX_FLASH_CAPACITY_GEOMETRY;
        if (sectors && sectors <= PROX_FLASH_MAX_VOLUME_BYTES / PROX_FLASH_SECTOR_BYTES)
            result = prox_flash_capacity(read_metadata, &fd, sectors * PROX_FLASH_SECTOR_BYTES, out);
    }
    return sceIoClose(fd) < 0 ? PROX_FLASH_CAPACITY_READ : result;
}
