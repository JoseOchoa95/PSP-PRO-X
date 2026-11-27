/* GPL-3.0-or-later */
#include <string.h>
#include <stdio.h>

#include "fat32_dirs.h"

struct InitialDirectory {
    const char *path;
    const char *short_name;
    uint8_t parent;
};

/* Indices are one-based. Zero refers to the FAT32 root. */
static const struct InitialDirectory directories[PSP_INITIAL_DIRECTORY_COUNT] = {
    {"/PSP", "PSP        ", 0},
    {"/PSP/GAME", "GAME       ", 1},
    {"/PSP/SAVEDATA", "SAVEDATA   ", 1},
    {"/PSP/COMMON", "COMMON     ", 1},
    {"/PSP/SYSTEM", "SYSTEM     ", 1},
    {"/MP_ROOT", "MP_ROOT    ", 0},
    {"/MP_ROOT/100MNV01", "100MNV01   ", 6},
    {"/MP_ROOT/101ANV01", "101ANV01   ", 6},
    {"/MUSIC", "MUSIC      ", 0},
    {"/PICTURE", "PICTURE    ", 0},
    {"/VIDEO", "VIDEO      ", 0},
    {"/PSP/THEME", "THEME      ", 1},
    {"/PSP/RSSCH", "RSSCH      ", 1},
    {"/PSP/RSSCH/IMPORT", "IMPORT     ", 13},
    /* The PRO-X menu adds these when Sony creates PSP/GAME. */
    {"/SEPLUGINS", "SEPLUG~1   ", 0},
    {"/ISO", "ISO        ", 0},
    {"/ISO/VIDEO", "VIDEO      ", 16},
};

/* Zero-length, read-only and hidden files from the menu-formatted reference. */
enum { DIR_ENTRY_BYTES = 32, INDICATOR_COUNT = 2 };
static const uint8_t indicators[INDICATOR_COUNT][DIR_ENTRY_BYTES] = {
    {'M', 'E', 'M', 'S', 'T', 'I', 'C', 'K', 'I', 'N', 'D', 0x03},
    {'M', 'S', 'T', 'K', '_', 'P', 'R', 'O', 'I', 'N', 'D', 0x03},
};

/* Sony writes this long-name entry before SEPLUG~1, with alias checksum 0xa3. */
static const uint8_t seplugins_long_name[DIR_ENTRY_BYTES] = {
    0x41, 'S', 0, 'E', 0, 'P', 0, 'L', 0, 'U', 0, 0x0f, 0, 0xa3,
    'G', 0, 'I', 0, 'N', 0, 'S', 0, 0, 0, 0xff, 0xff, 0, 0, 0xff, 0xff, 0xff, 0xff,
};

int fat32_ram_add_indicators(struct Fat32Ram *volume)
{
    struct Fat32Layout layout;
    uint8_t data[FAT32_SECTOR_BYTES];
    if (!fat32_plan(volume->length, &layout) || !fat32_ram_read(volume, layout.root_sector, data))
        return 0;
    for (unsigned offset = 0; offset < sizeof(data); offset += DIR_ENTRY_BYTES) {
        if (data[offset] && data[offset + 11] != 0x10 &&
            memcmp(data + offset, seplugins_long_name, DIR_ENTRY_BYTES))
            return 0;
        if (!data[offset]) {
            if (sizeof(data) - offset < sizeof(indicators) + DIR_ENTRY_BYTES)
                return 0;
            memcpy(data + offset, indicators, sizeof(indicators));
            memset(data + offset + sizeof(indicators), 0, DIR_ENTRY_BYTES);
            return fat32_ram_write(volume, layout.root_sector, data);
        }
    }
    return 0;
}

int fat32_ram_name_partition(struct Fat32Ram *volume, unsigned number)
{
    struct Fat32Layout layout;
    uint8_t root[FAT32_SECTOR_BYTES], boot[FAT32_SECTOR_BYTES], backup[FAT32_SECTOR_BYTES];
    char label[12];
    int length = snprintf(label, sizeof(label), "PART %u", number);
    if (!number || length < 1 || length >= (int)sizeof(label) ||
        !fat32_plan(volume->length, &layout) || !fat32_ram_read(volume, layout.root_sector, root) ||
        !fat32_ram_read(volume, 0, boot) || !fat32_ram_read(volume, 6, backup) ||
        memcmp(boot, backup, sizeof(boot)) || memcmp(boot + 71, "NO NAME    ", 11))
        return 0;
    memset(label + length, ' ', 11 - length);
    for (unsigned offset = 0; offset + 2 * DIR_ENTRY_BYTES <= sizeof(root); offset += DIR_ENTRY_BYTES) {
        if (root[offset + 11] == 0x08)
            return 0;
        if (!root[offset]) {
            memset(root + offset, 0, 2 * DIR_ENTRY_BYTES);
            memcpy(root + offset, label, 11);
            root[offset + 11] = 0x08;
            memcpy(boot + 71, label, 11);
            return fat32_ram_write(volume, layout.root_sector, root) &&
                   fat32_ram_write(volume, 6, boot) && fat32_ram_write(volume, 0, boot);
        }
    }
    return 0;
}

const char *fat32_trial_directory(unsigned index)
{
    return index < PSP_INITIAL_DIRECTORY_COUNT ? directories[index].path : NULL;
}

static uint32_t le16(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t le32(const uint8_t *p)
{
    return le16(p) | le16(p + 2) << 16;
}

static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(value >> (8 * i));
}

static int same_sectors(struct Fat32Ram *volume, uint32_t primary, uint32_t backup,
                        uint8_t sector[FAT32_SECTOR_BYTES])
{
    uint8_t other[FAT32_SECTOR_BYTES];
    return fat32_ram_read(volume, primary, sector) && fat32_ram_read(volume, backup, other) &&
           !memcmp(sector, other, sizeof(other));
}

static int record_child(uint32_t *clusters, unsigned parent, uint32_t cluster, const uint8_t *entry)
{
    unsigned child = 0;
    for (; child < PSP_INITIAL_DIRECTORY_COUNT; ++child) {
        if (directories[child].parent == parent && !memcmp(entry, directories[child].short_name, 11))
            break;
    }
    if (child == PSP_INITIAL_DIRECTORY_COUNT || clusters[child + 1])
        return 0;
    for (unsigned i = 0; i <= PSP_INITIAL_DIRECTORY_COUNT; ++i) {
        if (clusters[i] == cluster)
            return 0;
    }
    clusters[child + 1] = cluster;
    return 1;
}

static int scan_directory(struct Fat32Ram *volume, const struct Fat32Layout *layout,
                          uint32_t *clusters, unsigned index, const uint8_t *label)
{
    uint8_t data[FAT32_SECTOR_BYTES];
    uint32_t cluster = clusters[index];
    unsigned parent = index ? directories[index - 1].parent : 0;
    if (cluster < 2 || cluster > layout->clusters + 1 ||
        !fat32_ram_read(volume, 32 + cluster / 128, data) ||
        (le32(data + 4 * (cluster % 128)) & 0x0fffffff) < 0x0ffffff8)
        return 0;
    uint32_t lba = layout->root_sector + (cluster - 2) * layout->sectors_per_cluster;
    if (!fat32_ram_read(volume, lba, data))
        return 0;
    unsigned offset = 0;
    if (index) {
        uint32_t expected_parent = parent ? clusters[parent] : 0;
        if (memcmp(data, ".          ", 11) || memcmp(data + 32, "..         ", 11) ||
            data[11] != 0x10 || data[43] != 0x10 ||
            ((le16(data + 20) << 16) | le16(data + 26)) != cluster ||
            ((le16(data + 52) << 16) | le16(data + 58)) != expected_parent)
            return 0;
        offset = 64;
    }
    unsigned found_indicators = 0;
    int found_label = 0, expect_label = memcmp(label, "NO NAME    ", 11) != 0;
    /* The initial tree fits in the first sector of each directory cluster. */
    for (; offset < sizeof(data); offset += DIR_ENTRY_BYTES) {
        const uint8_t *entry = data + offset;
        if (!entry[0])
            return index || (found_indicators == (1u << INDICATOR_COUNT) - 1 && found_label == expect_label);
        if (!index && entry[11] == 0x08) {
            if (!expect_label || found_label || memcmp(entry, label, 11) || le32(entry + 28) ||
                le16(entry + 20) || le16(entry + 26))
                return 0;
            found_label = 1;
            continue;
        }
        int long_name = entry[11] == 0x0f;
        if (long_name) {
            if (index || offset + DIR_ENTRY_BYTES >= sizeof(data) ||
                memcmp(entry, seplugins_long_name, DIR_ENTRY_BYTES))
                return 0;
            offset += DIR_ENTRY_BYTES;
            entry = data + offset;
        }
        if (long_name != !memcmp(entry, "SEPLUG~1   ", 11))
            return 0;
        if (!index && entry[11] == 0x03) {
            unsigned indicator = 0;
            while (indicator < INDICATOR_COUNT && memcmp(entry, indicators[indicator], DIR_ENTRY_BYTES))
                ++indicator;
            if (indicator == INDICATOR_COUNT || found_indicators & (1u << indicator))
                return 0;
            found_indicators |= 1u << indicator;
            continue;
        }
        uint32_t child = le16(entry + 20) << 16 | le16(entry + 26);
        if (entry[11] != 0x10 || le32(entry + 28) || child < 2 || child > layout->clusters + 1 ||
            !record_child(clusters, index, child, entry))
            return 0;
    }
    return 0;
}

static int update_free_summary(struct Fat32Ram *volume, const struct Fat32Layout *layout)
{
    uint8_t sector[FAT32_SECTOR_BYTES];
    uint32_t free_clusters = 0, next_free = UINT32_MAX;
    for (uint32_t relative = 0; relative < layout->fat_sectors; ++relative) {
        if (!same_sectors(volume, 32 + relative, 32 + layout->fat_sectors + relative, sector))
            return 0;
        for (unsigned i = 0; i < 128; ++i) {
            uint32_t cluster = relative * 128 + i;
            if (cluster >= 2 && cluster <= layout->clusters + 1 &&
                !(le32(sector + i * 4) & 0x0fffffff)) {
                ++free_clusters;
                if (next_free == UINT32_MAX)
                    next_free = cluster;
            }
        }
    }
    if (free_clusters != layout->clusters - (PSP_INITIAL_DIRECTORY_COUNT + 1) ||
        !same_sectors(volume, 1, 7, sector) || le32(sector) != 0x41615252 ||
        le32(sector + 484) != 0x61417272 || le32(sector + 508) != 0xaa550000)
        return 0;
    put32(sector + 488, free_clusters);
    put32(sector + 492, next_free);
    uint8_t readback[FAT32_SECTOR_BYTES];
    return fat32_ram_write(volume, 7, sector) && fat32_ram_write(volume, 1, sector) &&
           same_sectors(volume, 1, 7, readback) && !memcmp(sector, readback, sizeof(sector));
}

int fat32_ram_finish_directories(struct Fat32Ram *volume)
{
    struct Fat32Layout layout;
    uint8_t boot[FAT32_SECTOR_BYTES];
    uint32_t clusters[PSP_INITIAL_DIRECTORY_COUNT + 1] = {2};
    if (!fat32_plan(volume->length, &layout) || !same_sectors(volume, 0, 6, boot) ||
        le32(boot + 28) != volume->start || le32(boot + 32) != volume->length ||
        le32(boot + 36) != layout.fat_sectors || le32(boot + 44) != 2)
        return 0;
    for (unsigned i = 0; i <= PSP_INITIAL_DIRECTORY_COUNT; ++i) {
        if (!scan_directory(volume, &layout, clusters, i, boot + 71))
            return 0;
    }
    return update_free_summary(volume, &layout);
}
