/* Count free entries in a bounded PSP flash0 FAT volume. GPL-3.0-or-later. */
#include "prox_flash_capacity.h"
#include <string.h>

#define SECTOR_BYTES PROX_FLASH_SECTOR_BYTES

static unsigned int le16(const unsigned char *data)
{
    return data[0] | (unsigned int)data[1] << 8;
}

static uint32_t le32(const unsigned char *data)
{
    return le16(data) | (uint32_t)le16(data + 2) << 16;
}

int prox_flash_capacity(ProxFlashRead read, void *context, uint32_t volume_bytes, ProxFlashCapacity *out)
{
    /* Three sectors align FAT12 groups to complete pairs of entries. */
    unsigned char table[3 * SECTOR_BYTES] __attribute__((aligned(64)));
    unsigned char mirror[3 * SECTOR_BYTES] __attribute__((aligned(64)));
    uint32_t total_sectors, total16, total32, data_start, clusters, fat_bytes;
    unsigned int cluster_sectors, reserved, fats, fat_sectors, root_entries, media;
    unsigned int bits, group, offset = 0, cluster = 0, free_clusters = 0;

    if (!read || !out || volume_bytes < SECTOR_BYTES || volume_bytes > PROX_FLASH_MAX_VOLUME_BYTES ||
        volume_bytes % SECTOR_BYTES)
        return PROX_FLASH_CAPACITY_GEOMETRY;
    if (read(context, 0, table, SECTOR_BYTES))
        return PROX_FLASH_CAPACITY_READ;

    cluster_sectors = table[13];
    reserved = le16(table + 14);
    fats = table[16];
    root_entries = le16(table + 17);
    total16 = le16(table + 19);
    media = table[21];
    fat_sectors = le16(table + 22);
    total32 = le32(table + 32);
    if (le16(table + 510) != 0xaa55 || le16(table + 11) != SECTOR_BYTES ||
        !cluster_sectors || cluster_sectors > 128 || (cluster_sectors & (cluster_sectors - 1)) ||
        !reserved || fats < 1 || fats > 2 || !root_entries || !fat_sectors ||
        (media != 0xf0 && media < 0xf8) || (!!total16 == !!total32))
        return PROX_FLASH_CAPACITY_GEOMETRY;

    total_sectors = total16 ? total16 : total32;
    data_start = reserved + fats * fat_sectors + (root_entries * 32u + SECTOR_BYTES - 1) / SECTOR_BYTES;
    if (data_start >= total_sectors || total_sectors > volume_bytes / SECTOR_BYTES)
        return PROX_FLASH_CAPACITY_GEOMETRY;
    clusters = (total_sectors - data_start) / cluster_sectors;
    if (!clusters || clusters >= 65525)
        return PROX_FLASH_CAPACITY_GEOMETRY;
    bits = clusters < 4085 ? 12 : 16;
    fat_bytes = fat_sectors * SECTOR_BYTES;
    if (((clusters + 2) * bits + 7) / 8 > fat_bytes)
        return PROX_FLASH_CAPACITY_GEOMETRY;
    group = bits == 12 ? sizeof(table) : SECTOR_BYTES;

    while (cluster < clusters + 2) {
        unsigned int bytes = fat_bytes - offset;
        if (bytes > group)
            bytes = group;
        uint32_t position = reserved * SECTOR_BYTES + offset;
        if (read(context, position, table, bytes) ||
            (fats == 2 && read(context, position + fat_bytes, mirror, bytes)))
            return PROX_FLASH_CAPACITY_READ;
        if (fats == 2 && memcmp(table, mirror, bytes))
            return PROX_FLASH_CAPACITY_FAT;

        unsigned int entries = bytes * 8 / bits;
        for (unsigned int i = 0; i < entries && cluster < clusters + 2; ++i, ++cluster) {
            unsigned int value;
            if (bits == 12) {
                value = le16(table + i + i / 2);
                value = i & 1 ? value >> 4 : value & 0xfff;
            } else {
                value = le16(table + i * 2);
            }

            if (cluster == 0) {
                if (value != ((bits == 12 ? 0xf00 : 0xff00) | media))
                    return PROX_FLASH_CAPACITY_FAT;
            } else if (cluster == 1) {
                /* FAT16 uses its two high bits to track volume state. */
                if (bits == 12 ? value < 0xff8 : (value & 0x3fff) != 0x3fff)
                    return PROX_FLASH_CAPACITY_FAT;
            } else if (!value) {
                free_clusters++;
            }
        }
        offset += bytes;
    }

    out->cluster_bytes = cluster_sectors * SECTOR_BYTES;
    out->total_bytes = clusters * out->cluster_bytes;
    out->free_bytes = free_clusters * out->cluster_bytes;
    out->fat_bits = bits;
    return PROX_FLASH_CAPACITY_OK;
}
