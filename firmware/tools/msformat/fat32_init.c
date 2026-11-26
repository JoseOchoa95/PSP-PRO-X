/* GPL-3.0-or-later */
#include <string.h>

#include "fat32_init.h"

enum {
    RESERVED = 32,
    FAT_COPIES = 2,
    ENTRIES_PER_SECTOR = FAT32_SECTOR_BYTES / 4,
    MIN_CLUSTERS = 65525,
    MAX_CLUSTERS = 0x0fffffee,
    FSINFO = 1,
    BACKUP = 6,
    ROOT_CLUSTER = 2,
};

int fat32_plan(uint32_t sectors, struct Fat32Layout *layout)
{
    if (sectors <= RESERVED)
        return 0;
    for (uint32_t spc = 64; spc; spc /= 2) {
        uint32_t divisor = ENTRIES_PER_SECTOR * spc + FAT_COPIES;
        uint32_t fat = ((uint64_t)sectors - RESERVED + 2 * spc + divisor - 1) / divisor;
        uint32_t data = RESERVED + FAT_COPIES * fat;
        if (data >= sectors)
            continue;
        uint32_t clusters = (sectors - data) / spc;
        if (clusters >= MIN_CLUSTERS && clusters <= MAX_CLUSTERS) {
            *layout = (struct Fat32Layout){sectors, fat, data, clusters, spc};
            return 1;
        }
    }
    return 0;
}

static void set16(uint8_t *field, uint32_t value)
{
    field[0] = value & 0xff;
    field[1] = (value >> 8) & 0xff;
}

static void set32(uint8_t *field, uint32_t value)
{
    set16(field, value);
    set16(field + 2, value >> 16);
}

static void make_boot(uint8_t *boot, uint32_t start, uint32_t serial, const struct Fat32Layout *layout)
{
    memset(boot, 0, FAT32_SECTOR_BYTES);
    memcpy(boot, "\xeb\x58\x90" "PROX    ", 11);
    set16(boot + 11, FAT32_SECTOR_BYTES);
    boot[13] = layout->sectors_per_cluster;
    set16(boot + 14, RESERVED);
    boot[16] = FAT_COPIES;
    boot[21] = 0xf8;
    set16(boot + 24, 63);
    set16(boot + 26, 255);
    set32(boot + 28, start);
    set32(boot + 32, layout->sectors);
    set32(boot + 36, layout->fat_sectors);
    set32(boot + 44, ROOT_CLUSTER);
    set16(boot + 48, FSINFO);
    set16(boot + 50, BACKUP);
    boot[64] = 0x80;
    boot[66] = 0x29;
    set32(boot + 67, serial);
    memcpy(boot + 71, "NO NAME    FAT32   ", 19);
    set16(boot + 510, 0xaa55);
}

enum Fat32InitResult fat32_initialize(uint32_t start, uint32_t sectors, uint32_t serial,
                                      Fat32WriteSector write, void *context)
{
    struct Fat32Layout layout;
    if (!write || !start || sectors > UINT32_MAX - start || !fat32_plan(sectors, &layout))
        return FAT32_INIT_INVALID_LAYOUT;

    uint8_t sector[FAT32_SECTOR_BYTES] = {0};
    /* Invalidate both old boot records before changing allocation metadata. */
    if (!write(context, 0, sector) || !write(context, BACKUP, sector))
        return FAT32_INIT_WRITE_FAILED;
    for (uint32_t lba = 1; lba < layout.root_sector + layout.sectors_per_cluster; ++lba) {
        if (lba == BACKUP)
            continue;
        memset(sector, 0, sizeof(sector));
        if (lba == FSINFO || lba == BACKUP + FSINFO) {
            set32(sector, 0x41615252);
            set32(sector + 484, 0x61417272);
            set32(sector + 488, layout.clusters - 1);
            set32(sector + 492, ROOT_CLUSTER + 1);
            set32(sector + 508, 0xaa550000);
        } else if (lba == RESERVED || lba == RESERVED + layout.fat_sectors) {
            set32(sector, 0x0ffffff8);
            set32(sector + 4, 0x0fffffff);
            set32(sector + 8, 0x0fffffff);
        }
        if (!write(context, lba, sector))
            return FAT32_INIT_WRITE_FAILED;
    }

    /* Publish the primary boot sector after all other metadata. */
    make_boot(sector, start, serial, &layout);
    if (!write(context, BACKUP, sector) || !write(context, 0, sector))
        return FAT32_INIT_WRITE_FAILED;
    return FAT32_INIT_OK;
}
