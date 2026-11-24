#include "prox_flash_capacity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { SECTOR_BYTES = 512, MAX_FAT_SECTORS = 24 };
static unsigned char metadata[(1 + 2 * MAX_FAT_SECTORS) * SECTOR_BYTES];
static unsigned int image_bytes, metadata_bytes, table_bytes, entries, faults, reads;

static void put16(unsigned char *data, unsigned int value)
{
    data[0] = value;
    data[1] = value >> 8;
}

static void entry(unsigned char *table, unsigned int bits, unsigned int index, unsigned int value)
{
    if (bits == 12) {
        unsigned char *pair = table + index + index / 2;
        if (index & 1) {
            pair[0] = (pair[0] & 15) | (value << 4);
            pair[1] = value >> 4;
        } else {
            pair[0] = value;
            pair[1] = (pair[1] & 0xf0) | (value >> 8);
        }
    } else {
        put16(table + index * 2, value);
    }
}

static void geometry(unsigned int bits, unsigned int clusters, unsigned int free_clusters,
                     unsigned int cluster_sectors, unsigned int fats)
{
    memset(metadata, 0, sizeof(metadata));
    unsigned int fat_sectors = ((clusters + 2) * bits + 8 * SECTOR_BYTES - 1) / (8 * SECTOR_BYTES);
    unsigned int total_sectors = 1 + fats * fat_sectors + 32 + clusters * cluster_sectors;
    assert(fat_sectors <= MAX_FAT_SECTORS);
    put16(metadata + 11, SECTOR_BYTES);
    metadata[13] = cluster_sectors;
    put16(metadata + 14, 1);
    metadata[16] = fats;
    put16(metadata + 17, 512);
    if (total_sectors <= 65535)
        put16(metadata + 19, total_sectors);
    else {
        put16(metadata + 32, total_sectors);
        put16(metadata + 34, total_sectors >> 16);
    }
    metadata[21] = 0xf8;
    put16(metadata + 22, fat_sectors);
    put16(metadata + 510, 0xaa55);
    unsigned int end = bits == 12 ? 0xfff : 0xffff;
    unsigned char *fat = metadata + SECTOR_BYTES;
    entry(fat, bits, 0, end - 7);
    entry(fat, bits, 1, end);
    /* Distribute free values throughout the table, including group boundaries. */
    for (unsigned int i = 0; i < clusters; ++i)
        if ((i * free_clusters) / clusters == ((i + 1) * free_clusters) / clusters)
            entry(fat, bits, i + 2, end);
    table_bytes = fat_sectors * SECTOR_BYTES;
    if (fats == 2)
        memcpy(fat + table_bytes, fat, table_bytes);
    image_bytes = total_sectors * SECTOR_BYTES;
    metadata_bytes = (1 + fats * fat_sectors) * SECTOR_BYTES;
    entries = clusters + 2;
    faults = 0;
    reads = 0;
}

static int read_metadata(void *context, uint32_t offset, void *output, unsigned int bytes)
{
    assert(context == metadata);
    assert(bytes && bytes <= 3 * SECTOR_BYTES && !(offset % SECTOR_BYTES) && !(bytes % SECTOR_BYTES));
    assert(offset < metadata_bytes && bytes <= metadata_bytes - offset);
    if (++reads == faults)
        return -1;
    memcpy(output, metadata + offset, bytes);
    return 0;
}

static void reject(int expected, unsigned int volume)
{
    ProxFlashCapacity result = {123, 45, 6, 7};
    assert(prox_flash_capacity(read_metadata, metadata, volume, &result) == expected);
    assert(result.total_bytes == 123 && result.free_bytes == 45 &&
           result.cluster_bytes == 6 && result.fat_bits == 7);
}

int main(void)
{
    ProxFlashCapacity result;
    geometry(12, 2619, 918, 32, 2);
    assert(prox_flash_capacity(read_metadata, metadata, image_bytes, &result) == PROX_FLASH_CAPACITY_OK);
    assert(result.fat_bits == 12 && result.cluster_bytes == 16384);
    assert(result.total_bytes == 42909696 && result.free_bytes == 15040512);
    assert(entries == 2621 && reads > 5);

    geometry(16, 5000, 1200, 8, 2);
    assert(!prox_flash_capacity(read_metadata, metadata, image_bytes, &result));
    assert(result.fat_bits == 16 && result.cluster_bytes == 4096);
    assert(result.total_bytes == 20480000 && result.free_bytes == 4915200);

    geometry(12, 2619, 0, 32, 1);
    assert(!prox_flash_capacity(read_metadata, metadata, image_bytes, &result) && result.free_bytes == 0);
    geometry(12, 2619, 2619, 32, 1);
    assert(!prox_flash_capacity(read_metadata, metadata, image_bytes, &result) && result.free_bytes == result.total_bytes);

    geometry(12, 2619, 918, 32, 2);
    metadata[SECTOR_BYTES + table_bytes + 17] ^= 1;
    reject(PROX_FLASH_CAPACITY_FAT, image_bytes);
    geometry(12, 2619, 918, 32, 2);
    faults = 4;
    reject(PROX_FLASH_CAPACITY_READ, image_bytes);
    geometry(12, 2619, 918, 32, 2);
    reject(PROX_FLASH_CAPACITY_GEOMETRY, image_bytes - SECTOR_BYTES);
    metadata[13] = 3;
    reject(PROX_FLASH_CAPACITY_GEOMETRY, image_bytes);
    geometry(12, 2619, 918, 32, 2);
    put16(metadata + 22, 1);
    reject(PROX_FLASH_CAPACITY_GEOMETRY, image_bytes);
    geometry(12, 2619, 918, 32, 2);
    metadata[510] = 0;
    reject(PROX_FLASH_CAPACITY_GEOMETRY, image_bytes);
    geometry(12, 2619, 918, 32, 1);
    metadata[SECTOR_BYTES] = 0;
    reject(PROX_FLASH_CAPACITY_FAT, image_bytes);
    puts("PASS: exact FAT12 and FAT16 counts; mirrored tables, bounded reads and invalid geometry rejected");
}
