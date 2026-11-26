/* GPL-3.0-or-later */
#ifndef MSFORMAT_FAT32_RAM_H
#define MSFORMAT_FAT32_RAM_H

#include "fat32_init.h"

enum { FAT32_RAM_SECTORS = 6 };
#ifdef MSDIRS_TRIAL
#define FAT32_RAM_CAPACITY 32
#else
#define FAT32_RAM_CAPACITY FAT32_RAM_SECTORS
#endif

struct Fat32Ram {
    uint32_t start, length, writes;
    unsigned used;
    struct {
        uint32_t lba;
        uint8_t sector[FAT32_SECTOR_BYTES];
    } entries[FAT32_RAM_CAPACITY];
};

int fat32_ram_write(void *context, uint32_t lba, const uint8_t sector[FAT32_SECTOR_BYTES]);
int fat32_ram_read(void *context, uint32_t lba, uint8_t sector[FAT32_SECTOR_BYTES]);
int fat32_ram_verify(struct Fat32Ram *volume);

#endif
