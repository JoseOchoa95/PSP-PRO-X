/* GPL-3.0-or-later */
#include <string.h>

#include "fat32_ram.h"

static const uint8_t zero[FAT32_SECTOR_BYTES];

static unsigned find_sector(const struct Fat32Ram *volume, uint32_t lba)
{
    unsigned i = 0;
    while (i < volume->used && volume->entries[i].lba != lba)
        ++i;
    return i;
}

int fat32_ram_write(void *context, uint32_t lba, const uint8_t sector[FAT32_SECTOR_BYTES])
{
    struct Fat32Ram *volume = context;
    if (lba >= volume->length)
        return 0;
    unsigned i = find_sector(volume, lba);
    if (!memcmp(sector, zero, sizeof(zero))) {
        if (i < volume->used)
            volume->entries[i] = volume->entries[--volume->used];
    } else {
        if (i == FAT32_RAM_CAPACITY)
            return 0;
        if (i == volume->used)
            ++volume->used;
        volume->entries[i].lba = lba;
        memcpy(volume->entries[i].sector, sector, FAT32_SECTOR_BYTES);
    }
    ++volume->writes;
    return 1;
}

int fat32_ram_read(void *context, uint32_t lba, uint8_t sector[FAT32_SECTOR_BYTES])
{
    const struct Fat32Ram *volume = context;
    if (lba >= volume->length)
        return 0;
    unsigned i = find_sector(volume, lba);
    memcpy(sector, i < volume->used ? volume->entries[i].sector : zero, FAT32_SECTOR_BYTES);
    return 1;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int matching_pair(struct Fat32Ram *volume, uint32_t a, uint32_t b, uint8_t *primary)
{
    uint8_t duplicate[FAT32_SECTOR_BYTES];
    return fat32_ram_read(volume, a, primary) && fat32_ram_read(volume, b, duplicate) &&
           !memcmp(primary, duplicate, sizeof(duplicate));
}

int fat32_ram_verify(struct Fat32Ram *volume)
{
    struct Fat32Layout layout;
    uint8_t sector[FAT32_SECTOR_BYTES];
    if (!fat32_plan(volume->length, &layout) || volume->used != FAT32_RAM_SECTORS ||
        volume->writes != layout.root_sector + layout.sectors_per_cluster + 2)
        return 0;
    if (!matching_pair(volume, 0, 6, sector) || sector[510] != 0x55 || sector[511] != 0xaa ||
        le32(sector + 28) != volume->start || le32(sector + 32) != volume->length ||
        le32(sector + 36) != layout.fat_sectors || le32(sector + 44) != 2 ||
        memcmp(sector + 82, "FAT32   ", 8))
        return 0;
    if (!matching_pair(volume, 1, 7, sector) || le32(sector) != 0x41615252 ||
        le32(sector + 484) != 0x61417272 || le32(sector + 488) != layout.clusters - 1 ||
        le32(sector + 508) != 0xaa550000)
        return 0;
    return matching_pair(volume, 32, 32 + layout.fat_sectors, sector) &&
           le32(sector) == 0x0ffffff8 && le32(sector + 4) == 0x0fffffff &&
           le32(sector + 8) == 0x0fffffff;
}
