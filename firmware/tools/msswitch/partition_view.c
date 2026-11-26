/* GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>

#include "partition_view.h"
#include "probe.h"

static uint32_t decode32(const unsigned char *bytes)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= (uint32_t)bytes[i] << (8 * i);
    return value;
}

size_t read_primary_views(const unsigned char *mbr, uint32_t medium_sectors,
                          struct PartitionView views[MBR_PARTITIONS])
{
    static const unsigned char blank[16];
    struct PartitionView parsed[MBR_PARTITIONS];
    size_t count = 0;
    if (mbr[510] != 0x55 || mbr[511] != 0xaa)
        return 0;
    for (unsigned i = 0; i < MBR_PARTITIONS; ++i) {
        const unsigned char *entry = mbr + 446 + 16 * i;
        uint32_t start = decode32(entry + 8);
        uint32_t sectors = decode32(entry + 12);
        if (!memcmp(entry, blank, sizeof(blank)))
            continue;
        if ((entry[0] != 0 && entry[0] != 0x80) || (entry[4] != 0x0b && entry[4] != 0x0c) ||
            !sectors || !start || start >= medium_sectors || sectors > medium_sectors - start)
            return 0;
        for (size_t j = 0; j < count; ++j) {
            if (start < parsed[j].start + parsed[j].sectors && parsed[j].start < start + sectors)
                return 0;
        }
        parsed[count++] = (struct PartitionView){start, sectors};
    }
    memcpy(views, parsed, count * sizeof(*views));
    return count;
}

int view_sector(const struct PartitionView *view, uint64_t local_lba, uint32_t *absolute_lba)
{
    if (local_lba >= view->sectors || local_lba > UINT32_MAX - view->start)
        return 0;
    *absolute_lba = view->start + (uint32_t)local_lba;
    return 1;
}

int64_t view_seek(const struct PartitionView *view, int64_t position, int64_t offset, int whence)
{
    int64_t limit = (int64_t)view->sectors * SECTOR_BYTES;
    int64_t base;
    switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = position; break;
    case SEEK_END: base = limit; break;
    default: return -1;
    }
    if (base < 0 || base > limit || offset < -base || offset > limit - base || offset % SECTOR_BYTES)
        return -1;
    return base + offset;
}
