/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "partition_view.h"
#include "probe.h"

static void encode32(unsigned char *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (unsigned char)(value >> (8 * i));
}

static void partition(unsigned char *mbr, unsigned slot, uint32_t start, uint32_t sectors)
{
    unsigned char *entry = mbr + 446 + slot * 16;
    entry[4] = 0x0c;
    encode32(entry + 8, start);
    encode32(entry + 12, sectors);
}

int main(void)
{
    const uint32_t capacity = 500695040;
    unsigned char mbr[SECTOR_BYTES] = {0};
    struct PartitionView views[MBR_PARTITIONS];
    uint32_t absolute;

    mbr[510] = 0x55;
    mbr[511] = 0xaa;
    mbr[446] = 0x80;
    partition(mbr, 0, 2048, 230686720);
    partition(mbr, 1, 230688768, 230686720);
    partition(mbr, 2, 461375488, 39319552);
    assert(read_primary_views(mbr, capacity, views) == 3);
    assert(view_sector(&views[0], 0, &absolute) && absolute == 2048);
    assert(view_sector(&views[1], 37746688, &absolute) && absolute == 268435456);
    assert(view_sector(&views[2], 39318528, &absolute) && absolute == 500694016);
    assert(view_sector(&views[2], 39319551, &absolute) && absolute == capacity - 1);
    assert(!view_sector(&views[2], 39319552, &absolute));
    assert(!view_sector(&views[2], UINT64_MAX, &absolute));
    struct PartitionView overflow = {UINT32_MAX, 2};
    assert(!view_sector(&overflow, 1, &absolute));

    int64_t end = (int64_t)views[1].sectors * SECTOR_BYTES;
    assert(end > UINT32_MAX);
    assert(view_seek(&views[1], 0, 0, SEEK_END) == end);
    assert(view_seek(&views[1], 0, -SECTOR_BYTES, SEEK_END) == end - SECTOR_BYTES);
    assert(view_seek(&views[1], end - SECTOR_BYTES, SECTOR_BYTES, SEEK_CUR) == end);
    assert(view_seek(&views[1], end, SECTOR_BYTES, SEEK_CUR) < 0);
    assert(view_seek(&views[1], 0, -SECTOR_BYTES, SEEK_SET) < 0);
    assert(view_seek(&views[1], 0, 1, SEEK_SET) < 0);
    assert(view_seek(&views[1], 0, INT64_MAX, SEEK_END) < 0);
    assert(view_seek(&views[1], 0, INT64_MIN, SEEK_END) < 0);
    assert(view_seek(&views[1], 0, 0, -1) < 0);

    unsigned char invalid[SECTOR_BYTES];
    memcpy(invalid, mbr, sizeof(invalid));
    invalid[511] = 0;
    assert(!read_primary_views(invalid, capacity, views));
    memcpy(invalid, mbr, sizeof(invalid));
    invalid[450] = 0xee;
    assert(!read_primary_views(invalid, capacity, views));
    assert(!read_primary_views(mbr, capacity - 1, views));
    memcpy(invalid, mbr, sizeof(invalid));
    partition(invalid, 1, 230688767, 230686720);
    assert(!read_primary_views(invalid, capacity, views));
    memcpy(invalid, mbr, sizeof(invalid));
    partition(invalid, 3, 0, 1);
    assert(!read_primary_views(invalid, capacity, views));
    memcpy(invalid, mbr, sizeof(invalid));
    partition(invalid, 3, capacity - 1, UINT32_MAX);
    assert(!read_primary_views(invalid, capacity, views));
    puts("PASS: partition mapping, 64-bit seeks, device limits and invalid MBRs");
}
