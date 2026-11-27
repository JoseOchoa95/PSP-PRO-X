/* GPL-3.0-or-later */
#include <string.h>

#include "fat32_dirs.h"
#include "gpt.h"

enum { HEADER_BYTES = 92, ENTRY_BYTES = 128, MAX_ENTRY_SECTORS = 1024, TABLE_OFFSET = 446 };

static uint32_t read32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint64_t read64(const uint8_t *data)
{
    return (uint64_t)read32(data) | ((uint64_t)read32(data + 4) << 32);
}

static uint32_t crc_bytes(uint32_t crc, const uint8_t *data, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
    }
    return crc;
}

static int protective_mbr(const uint8_t *mbr, uint32_t last)
{
    static const uint8_t empty[16] = {0};
    unsigned matches = 0;
    if (mbr[510] != 0x55 || mbr[511] != 0xaa)
        return 0;
    for (unsigned i = 0; i < 4; ++i) {
        const uint8_t *entry = mbr + TABLE_OFFSET + i * sizeof(empty);
        if (!memcmp(entry, empty, sizeof(empty)))
            continue;
        uint32_t length = read32(entry + 12);
        /* Accept a saturated protective length. Both GPT headers still have
           to match the independently measured device boundary. */
        if (entry[0] || entry[4] != 0xee || read32(entry + 8) != 1 || (length != last && length != UINT32_MAX))
            return 0;
        ++matches;
    }
    return matches == 1;
}

static int valid_header(uint8_t *header, uint32_t own, uint32_t alternate, uint32_t *checksum)
{
    uint32_t bytes = read32(header + 12);
    *checksum = read32(header + 16);
    if (memcmp(header, "EFI PART", 8) || read32(header + 8) != 0x10000 ||
        bytes < HEADER_BYTES || bytes > FAT32_SECTOR_BYTES || read32(header + 20) ||
        read64(header + 24) != own || read64(header + 32) != alternate)
        return 0;
    memset(header + 16, 0, 4);
    return ~crc_bytes(UINT32_MAX, header, bytes) == *checksum;
}

static int matching_entries(MsReadSector read, void *device, uint32_t backup_start, uint32_t bytes,
                            uint32_t checksum)
{
    uint8_t first[FAT32_SECTOR_BYTES], second[FAT32_SECTOR_BYTES];
    uint32_t crc = UINT32_MAX;
    for (uint32_t offset = 0; bytes; ++offset) {
        unsigned count = bytes < sizeof(first) ? bytes : sizeof(first);
        if (!read(device, 2 + offset, first) || !read(device, backup_start + offset, second) ||
            memcmp(first, second, count))
            return 0;
        crc = crc_bytes(crc, first, count);
        bytes -= count;
    }
    return ~crc == checksum;
}

int ms_gpt_inspect(const struct MsFormatPlan *plan, MsReadSector read, void *device, struct MsGptLayout *gpt)
{
    struct MsFormatVolume first, last;
    struct Fat32Layout fat;
    if (!gpt)
        return 0;
    *gpt = (struct MsGptLayout){0};
    if (!plan || !read || !plan->volume_count || !ms_format_volume(plan, 0, &first) ||
        !ms_format_volume(plan, plan->volume_count - 1, &last) || !fat32_plan(last.sectors, &fat))
        return 0;

    uint32_t end = plan->usable_sectors - 1;
    uint8_t primary[FAT32_SECTOR_BYTES], backup[FAT32_SECTOR_BYTES];
    struct MsGptLayout layout = {0};
    if (!read(device, 0, primary) || !protective_mbr(primary, end) ||
        !read(device, 1, primary) || !valid_header(primary, 1, end, &layout.primary_crc) ||
        !read(device, end, backup) || !valid_header(backup, end, 1, &layout.backup_crc) ||
        memcmp(primary + 40, backup + 40, 32) || memcmp(primary + 80, backup + 80, 12))
        return 0;

    uint32_t entry_size = read32(primary + 84);
    uint64_t array_bytes = (uint64_t)read32(primary + 80) * entry_size;
    if (entry_size < ENTRY_BYTES || (entry_size & (entry_size - 1)) || array_bytes < 16384 ||
        array_bytes > MAX_ENTRY_SECTORS * FAT32_SECTOR_BYTES)
        return 0;
    uint32_t sectors = (array_bytes + FAT32_SECTOR_BYTES - 1) / FAT32_SECTOR_BYTES;
    uint32_t metadata_end = last.start + fat.root_sector + (PSP_INITIAL_DIRECTORY_COUNT + 1) * fat.sectors_per_cluster;
    uint64_t first_usable = read64(primary + 40), last_usable = read64(primary + 48);
    if (sectors >= end)
        return 0;
    layout.backup_start = end - sectors;
    layout.sectors_per_copy = sectors + 1;
    if (read64(primary + 72) != 2 || read64(backup + 72) != layout.backup_start ||
        first_usable < 2 + sectors || first_usable > last_usable || last_usable >= layout.backup_start ||
        layout.sectors_per_copy >= first.start || layout.backup_start < metadata_end ||
        !matching_entries(read, device, layout.backup_start, (uint32_t)array_bytes, read32(primary + 88)))
        return 0;
    *gpt = layout;
    return 1;
}
