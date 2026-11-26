/* GPL-3.0-or-later */
#ifndef MSFORMAT_FAT32_INIT_H
#define MSFORMAT_FAT32_INIT_H

#include <stdint.h>

enum { FAT32_SECTOR_BYTES = 512 };

struct Fat32Layout {
    uint32_t sectors, fat_sectors, root_sector, clusters, sectors_per_cluster;
};

typedef int (*Fat32WriteSector)(void *context, uint32_t lba,
                               const uint8_t sector[FAT32_SECTOR_BYTES]);

enum Fat32InitResult {
    FAT32_INIT_OK,
    FAT32_INIT_INVALID_LAYOUT,
    FAT32_INIT_WRITE_FAILED,
};

int fat32_plan(uint32_t sectors, struct Fat32Layout *layout);

/* The callback is synchronous and receives a partition-relative sector. */
enum Fat32InitResult fat32_initialize(uint32_t start, uint32_t sectors, uint32_t serial,
                                      Fat32WriteSector write, void *context);

#endif
