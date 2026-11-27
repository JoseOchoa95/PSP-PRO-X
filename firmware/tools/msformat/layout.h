/* GPL-3.0-or-later */
#ifndef MSFORMAT_LAYOUT_H
#define MSFORMAT_LAYOUT_H

#include "fat32_init.h"

enum MsFormatLayoutResult {
    MS_FORMAT_INVALID,
    MS_FORMAT_STOCK,
    MS_FORMAT_EXTENDED,
};

struct MsFormatPlan {
    uint32_t usable_sectors, full_sectors, final_sectors;
    unsigned volume_count, primary_count;
};

struct MsFormatVolume {
    uint32_t start, sectors, ebr;
};

typedef int (*MsReadSector)(void *device, uint32_t lba, uint8_t sector[FAT32_SECTOR_BYTES]);

/* The caller supplies a verified extent or a verified prefix at least as large as the cap. */
enum MsFormatLayoutResult ms_format_layout(uint64_t reported_sectors, uint64_t verified_sectors,
                                           struct MsFormatPlan *plan);
int ms_format_volume(const struct MsFormatPlan *plan, unsigned index, struct MsFormatVolume *volume);

/* LBA 0 supplies the MBR. Logical volumes carry their own EBR LBA. */
int ms_format_table_sector(const struct MsFormatPlan *plan, uint32_t lba,
                           uint8_t sector[FAT32_SECTOR_BYTES]);

#endif
