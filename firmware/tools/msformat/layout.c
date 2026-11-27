/* GPL-3.0-or-later */
#include <string.h>

#include "../msdiag/capacity.h"
#include "layout.h"

enum {
    ALIGNMENT = 2048,
    SECTORS_PER_GIB = 2097152,
    MBR_ENTRIES = 4,
    TABLE_OFFSET = 446,
    ENTRY_BYTES = 16,
    FAT32_LBA = 0x0c,
    EXTENDED_LBA = 0x0f,
};

static uint64_t volume_start(uint64_t full, unsigned primary, unsigned index)
{
    uint64_t table_sectors = index >= primary ? (uint64_t)(index - primary + 1) * ALIGNMENT : 0;
    return ALIGNMENT + index * full + table_sectors;
}

enum MsFormatLayoutResult ms_format_layout(uint64_t reported_sectors, uint64_t verified_sectors,
                                           struct MsFormatPlan *plan)
{
    if (!plan)
        return MS_FORMAT_INVALID;
    *plan = (struct MsFormatPlan){0};
    if (!reported_sectors || reported_sectors > UINT32_MAX || reported_sectors > verified_sectors)
        return MS_FORMAT_INVALID;
    if (verified_sectors <= UINT64_C(128000000000) / FAT32_SECTOR_BYTES)
        return MS_FORMAT_STOCK;

    const uint64_t cap = CAPACITY_LIMIT_BYTES / FAT32_SECTOR_BYTES;
    uint64_t usable = verified_sectors < cap ? verified_sectors : cap;
    uint64_t full = reported_sectors / SECTORS_PER_GIB * SECTORS_PER_GIB;
    struct Fat32Layout fat;
    if (!full || !fat32_plan((uint32_t)full, &fat))
        return MS_FORMAT_INVALID;

    /* Reserve the fourth MBR slot for an extended chain only when its second
       logical volume can fit. Otherwise use all four slots for primary data. */
    uint64_t fifth = volume_start(full, MBR_ENTRIES - 1, MBR_ENTRIES);
    unsigned primary = MBR_ENTRIES;
    if (fifth < usable && fat32_plan((uint32_t)(usable - fifth), &fat))
        --primary;

    unsigned count = 0;
    uint32_t final = 0;
    for (;;) {
        uint64_t start = volume_start(full, primary, count);
        if (start >= usable || (primary == MBR_ENTRIES && count >= MBR_ENTRIES))
            break;
        uint32_t available = (uint32_t)(usable - start);
        if (available < full) {
            if (fat32_plan(available, &fat)) {
                final = available;
                ++count;
            }
            break;
        }
        ++count;
    }
    if (!count)
        return MS_FORMAT_INVALID;
    *plan = (struct MsFormatPlan){(uint32_t)usable, (uint32_t)full, final, count,
                                 count < primary ? count : primary};
    return MS_FORMAT_EXTENDED;
}

int ms_format_volume(const struct MsFormatPlan *plan, unsigned index, struct MsFormatVolume *volume)
{
    if (!plan || !volume || !plan->full_sectors || !plan->primary_count || index >= plan->volume_count)
        return 0;
    uint64_t start = volume_start(plan->full_sectors, plan->primary_count, index);
    uint32_t sectors = index + 1 == plan->volume_count && plan->final_sectors
                           ? plan->final_sectors : plan->full_sectors;
    if (start + sectors > plan->usable_sectors)
        return 0;
    *volume = (struct MsFormatVolume){(uint32_t)start, sectors,
                                      index >= plan->primary_count ? (uint32_t)start - ALIGNMENT : 0};
    return 1;
}

static void put32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        out[i] = (uint8_t)(value >> (i * 8));
}

static void partition_entry(uint8_t *entry, uint8_t type, uint32_t start, uint32_t sectors)
{
    /* Saturated CHS fields leave the LBAs authoritative for types 0x0c and 0x0f. */
    const uint8_t chs[3] = {0xfe, 0xff, 0xff};
    memcpy(entry + 1, chs, sizeof(chs));
    entry[4] = type;
    memcpy(entry + 5, chs, sizeof(chs));
    put32(entry + 8, start);
    put32(entry + 12, sectors);
}

int ms_format_table_sector(const struct MsFormatPlan *plan, uint32_t lba,
                           uint8_t sector[FAT32_SECTOR_BYTES])
{
    struct MsFormatVolume last, first_logical, volume;
    if (!plan || !sector || !plan->volume_count || plan->primary_count > MBR_ENTRIES ||
        !ms_format_volume(plan, plan->volume_count - 1, &last))
        return 0;
    int extended = plan->volume_count > plan->primary_count;
    if (extended && (plan->primary_count != MBR_ENTRIES - 1 ||
                     !ms_format_volume(plan, plan->primary_count, &first_logical)))
        return 0;

    memset(sector, 0, FAT32_SECTOR_BYTES);
    uint8_t *entry = sector + TABLE_OFFSET;
    if (!lba) {
        entry[0] = 0x80;
        for (unsigned i = 0; i < plan->primary_count; ++i, entry += ENTRY_BYTES) {
            if (!ms_format_volume(plan, i, &volume))
                return 0;
            partition_entry(entry, FAT32_LBA, volume.start, volume.sectors);
        }
        /* The first aligned LBA is representable in BIOS geometry (255 heads, 63 sectors). */
        sector[TABLE_OFFSET + 1] = 32;
        sector[TABLE_OFFSET + 2] = 33;
        sector[TABLE_OFFSET + 3] = 0;
        if (extended)
            partition_entry(entry, EXTENDED_LBA, first_logical.ebr,
                            last.start + last.sectors - first_logical.ebr);
    } else {
        if (!extended || lba < first_logical.ebr)
            return 0;
        uint32_t distance = lba - first_logical.ebr;
        uint64_t stride = (uint64_t)plan->full_sectors + ALIGNMENT;
        unsigned i = plan->primary_count + distance / stride;
        if (distance % stride || !ms_format_volume(plan, i, &volume) || volume.ebr != lba)
            return 0;
        partition_entry(entry, FAT32_LBA, volume.start - lba, volume.sectors);
        if (i + 1 < plan->volume_count) {
            if (!ms_format_volume(plan, i + 1, &volume))
                return 0;
            partition_entry(entry + ENTRY_BYTES, EXTENDED_LBA, volume.ebr - first_logical.ebr,
                            last.start + last.sectors - volume.ebr);
        }
    }
    sector[510] = 0x55;
    sector[511] = 0xaa;
    return 1;
}
