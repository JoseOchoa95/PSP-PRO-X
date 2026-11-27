/* GPL-3.0-or-later */
#include <string.h>

#include "fat32_probe.h"

enum {
    FAT32_ENTRIES_PER_SECTOR = SECTOR_BYTES / 4,
    ROOT_SECTOR_BUDGET = 256,
    ROOT_CLUSTER = 2,
    TRIAL_CLUSTER_SECTORS = 64,
    TRIAL_RESERVED = 64,
    TRIAL_FAT_SECTORS = 4864,
    TRIAL_DATA_START = TRIAL_RESERVED + 2 * TRIAL_FAT_SECTORS,
    TRIAL_FILE_CLUSTER = ROOT_CLUSTER + (FAT32_TRIAL_FILE_SECTOR - TRIAL_DATA_START) / TRIAL_CLUSTER_SECTORS,
};

static const uint32_t fat_end = 0x0ffffff8;
static const unsigned char name[11] = "PXPROBE BIN";

struct FatVolume {
    uint32_t fat_start, data_start, clusters, sectors_per_cluster, root;
};

static uint32_t read16(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8;
}

static uint32_t read32(const unsigned char *p)
{
    return read16(p) | read16(p + 2) << 16;
}

static int data_cluster(const struct FatVolume *volume, uint32_t cluster)
{
    return cluster >= ROOT_CLUSTER && cluster <= volume->clusters + 1;
}

static int parse_volume(struct FatVolume *volume, const unsigned char *boot, uint32_t limit)
{
    uint32_t total = read32(boot + 32), fat_size = read32(boot + 36);
    uint32_t reserved = read16(boot + 14), flags = read16(boot + 40);
    uint32_t spc = boot[13], fats = boot[16], root = read32(boot + 44);
    uint32_t active = flags & 0x80 ? flags & 15 : 0;
    uint64_t data_start = (uint64_t)reserved + (uint64_t)fats * fat_size;
    if (read16(boot + 11) != SECTOR_BYTES || !spc || spc > 128 || (spc & (spc - 1)) ||
        !reserved || !fat_size || fats < 1 || fats > 2 || active >= fats ||
        read16(boot + 17) || read16(boot + 19) || read16(boot + 22) || read16(boot + 42) ||
        total > limit || data_start >= total)
        return 0;
    uint32_t clusters = (total - (uint32_t)data_start) / spc;
    if (clusters < 65525 || clusters > 0x0fffffee ||
        (uint64_t)fat_size * FAT32_ENTRIES_PER_SECTOR < clusters + 2)
        return 0;
    *volume = (struct FatVolume){reserved + active * fat_size, (uint32_t)data_start, clusters, spc, root};
    return data_cluster(volume, root);
}

static int is_fat32_boot(const unsigned char *boot)
{
    return read16(boot + 510) == 0xaa55 &&
           (boot[0] == 0xe9 || (boot[0] == 0xeb && boot[2] == 0x90)) &&
           !read16(boot + 17) && !read16(boot + 22) && read32(boot + 36) &&
           memcmp(boot + 3, "EXFAT   ", 8);
}

int fat32_matches_extent(const unsigned char boot[SECTOR_BYTES], uint32_t start, uint32_t sectors)
{
    struct FatVolume volume;
    return is_fat32_boot(boot) && read32(boot + 28) == start &&
           read32(boot + 32) == sectors && parse_volume(&volume, boot, sectors);
}

static uint32_t cluster_start(const struct FatVolume *volume, uint32_t cluster)
{
    return volume->data_start + (cluster - ROOT_CLUSTER) * volume->sectors_per_cluster;
}

static int read_next(ReadFatSector read, void *context, const struct FatVolume *volume,
                     uint32_t cluster, unsigned char *sector, uint32_t *next)
{
    if (!read(context, volume->fat_start + cluster / FAT32_ENTRIES_PER_SECTOR, sector))
        return 0;
    *next = read32(sector + (cluster % FAT32_ENTRIES_PER_SECTOR) * 4) & 0x0fffffff;
    return 1;
}

int fat32_read_volume_identity(ReadFatSector read, void *context, struct Fat32VolumeIdentity *identity)
{
    unsigned char buffer[SECTOR_BYTES] __attribute__((aligned(64)));
    struct FatVolume volume;
    if (!read(context, 0, buffer) || !is_fat32_boot(buffer) ||
        !parse_volume(&volume, buffer, read32(buffer + 32)))
        return 0;
    *identity = (struct Fat32VolumeIdentity){.start = read32(buffer + 28),
        .sectors = read32(buffer + 32), .serial = read32(buffer + 67)};
    uint32_t cluster = volume.root;
    unsigned budget = ROOT_SECTOR_BUDGET;
    while (budget && data_cluster(&volume, cluster)) {
        for (uint32_t i = 0; i < volume.sectors_per_cluster && budget; ++i, --budget) {
            if (!read(context, cluster_start(&volume, cluster) + i, buffer))
                return 0;
            for (unsigned offset = 0; offset < SECTOR_BYTES; offset += 32) {
                const unsigned char *entry = buffer + offset;
                if (!entry[0])
                    return 1;
                if (entry[0] == 0xe5 || entry[11] != 0x08)
                    continue;
                unsigned length = 11;
                while (length && entry[length - 1] == ' ')
                    --length;
                for (unsigned j = 0; j < length; ++j)
                    identity->label[j] = entry[j] >= 0x20 && entry[j] <= 0x7e ? (char)entry[j] : '?';
                return 1;
            }
        }
        uint32_t next;
        if (!budget || !read_next(read, context, &volume, cluster, buffer, &next) || next == cluster)
            return 0;
        if (next >= fat_end)
            return 1;
        cluster = next;
    }
    return 0;
}

enum Fat32ProbeResult fat32_read_probe(ReadFatSector read, void *context, uint32_t sectors,
                                     unsigned char file[SECTOR_BYTES])
{
    unsigned char buffer[SECTOR_BYTES];
    struct FatVolume volume;
    if (!sectors || !read(context, 0, buffer))
        return FAT32_PROBE_READ_ERROR;
    if (!is_fat32_boot(buffer))
        return FAT32_PROBE_NO_VOLUME;
    if (!parse_volume(&volume, buffer, sectors))
        return FAT32_PROBE_BAD_LAYOUT;
    uint32_t cluster = volume.root;
    unsigned budget = ROOT_SECTOR_BUDGET;
    while (budget && data_cluster(&volume, cluster)) {
        for (uint32_t i = 0; i < volume.sectors_per_cluster; ++i) {
            if (!budget--)
                return FAT32_PROBE_BAD_LAYOUT;
            if (!read(context, cluster_start(&volume, cluster) + i, buffer))
                return FAT32_PROBE_READ_ERROR;
            for (unsigned offset = 0; offset < SECTOR_BYTES; offset += 32) {
                const unsigned char *entry = buffer + offset;
                if (!entry[0])
                    return FAT32_PROBE_NO_FILE;
                if (entry[0] == 0xe5 || (entry[11] & 0x18) || memcmp(entry, name, sizeof(name)))
                    continue;
                uint32_t data = (read16(entry + 20) << 16) | read16(entry + 26);
                if (read32(entry + 28) != SECTOR_BYTES || !data_cluster(&volume, data) || data == cluster)
                    return FAT32_PROBE_BAD_LAYOUT;
                uint32_t end;
                if (!read_next(read, context, &volume, data, buffer, &end))
                    return FAT32_PROBE_READ_ERROR;
                if (end < fat_end)
                    return FAT32_PROBE_BAD_LAYOUT;
                return read(context, cluster_start(&volume, data), file)
                    ? FAT32_PROBE_MATCHED_FILE : FAT32_PROBE_READ_ERROR;
            }
        }
        uint32_t next;
        if (!read_next(read, context, &volume, cluster, buffer, &next))
            return FAT32_PROBE_READ_ERROR;
        if (next >= fat_end)
            return FAT32_PROBE_NO_FILE;
        if (next == cluster)
            return FAT32_PROBE_BAD_LAYOUT;
        cluster = next;
    }
    return FAT32_PROBE_BAD_LAYOUT;
}

static void put16(unsigned char *p, uint32_t value)
{
    p[0] = value & 0xff;
    p[1] = (value >> 8) & 0xff;
}

static void put32(unsigned char *p, uint32_t value)
{
    put16(p, value);
    put16(p + 2, value >> 16);
}

static void trial_boot(unsigned char *sector)
{
    sector[0] = 0xeb;
    sector[1] = 0x58;
    sector[2] = 0x90;
    memcpy(sector + 3, "MSWIN4.1", 8);
    put16(sector + 11, SECTOR_BYTES);
    sector[13] = TRIAL_CLUSTER_SECTORS;
    put16(sector + 14, TRIAL_RESERVED);
    sector[16] = 2;
    sector[21] = 0xf8;
    put32(sector + 32, FAT32_TRIAL_SECTORS);
    put32(sector + 36, TRIAL_FAT_SECTORS);
    put32(sector + 44, ROOT_CLUSTER);
    put16(sector + 48, 1);
    put16(sector + 50, 6);
    sector[64] = 0x80;
    sector[66] = 0x29;
    memcpy(sector + 71, "PROX TEST  FAT32   ", 19);
    put16(sector + 510, 0xaa55);
}

int fat32_trial_read(void *context, uint32_t lba, unsigned char sector[SECTOR_BYTES])
{
    struct Fat32Trial *trial = context;
    if (lba >= FAT32_TRIAL_SECTORS)
        return 0;
    if (lba == FAT32_TRIAL_FILE_SECTOR) {
        trial->backing_reads++;
        return trial->backing_read(trial->backing_context, lba, sector);
    }
    memset(sector, 0, SECTOR_BYTES);
    if (lba == 0 || lba == 6) {
        trial_boot(sector);
    } else if (lba == 1 || lba == 7) {
        put32(sector, 0x41615252);
        put32(sector + 484, 0x61417272);
        put32(sector + 488, UINT32_MAX);
        put32(sector + 492, UINT32_MAX);
        put32(sector + 508, 0xaa550000);
    } else if (lba == TRIAL_DATA_START) {
        memcpy(sector, name, sizeof(name));
        sector[11] = 0x20;
        put16(sector + 20, TRIAL_FILE_CLUSTER >> 16);
        put16(sector + 26, TRIAL_FILE_CLUSTER);
        put32(sector + 28, SECTOR_BYTES);
    } else if (lba >= TRIAL_RESERVED && lba < TRIAL_DATA_START) {
        uint32_t index = (lba - TRIAL_RESERVED) % TRIAL_FAT_SECTORS;
        if (!index) {
            put32(sector, fat_end);
            put32(sector + 4, 0x0fffffff);
            put32(sector + 8, 0x0fffffff);
        }
        if (index == TRIAL_FILE_CLUSTER / FAT32_ENTRIES_PER_SECTOR)
            put32(sector + 4 * (TRIAL_FILE_CLUSTER % FAT32_ENTRIES_PER_SECTOR), 0x0fffffff);
    }
    return 1;
}
