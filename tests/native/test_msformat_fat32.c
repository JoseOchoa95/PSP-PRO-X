/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fat32_init.h"
#include "fat32_ram.h"

enum { TEST_START = 461375488, TEST_SECTORS = 39319552 };

struct FormatSink {
    struct Fat32Layout layout;
    uint32_t writes, stop_at, last;
    unsigned published;
    FILE *journal;
    uint8_t boot[FAT32_SECTOR_BYTES];
};

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int inspect_sector(void *context, uint32_t lba, const uint8_t *sector)
{
    struct FormatSink *sink = context;
    assert(lba < sink->layout.sectors);
    assert(lba < sink->layout.root_sector + sink->layout.sectors_per_cluster);
    if (sink->writes++ == sink->stop_at)
        return 0;
    if (lba == 0 && sector[510] == 0x55 && sector[511] == 0xaa) {
        assert(sink->last == 6 && !memcmp(sink->boot, sector, FAT32_SECTOR_BYTES));
        sink->published++;
    } else if (lba == 6) {
        memcpy(sink->boot, sector, sizeof(sink->boot));
    }
    if (sink->journal) {
        const unsigned char encoded[] = {lba & 0xff, (lba >> 8) & 0xff,
                                         (lba >> 16) & 0xff, (lba >> 24) & 0xff};
        assert(fwrite(encoded, 1, sizeof(encoded), sink->journal) == sizeof(encoded));
        assert(fwrite(sector, 1, FAT32_SECTOR_BYTES, sink->journal) == FAT32_SECTOR_BYTES);
    }
    sink->last = lba;
    return 1;
}

int main(int argc, char **argv)
{
    static const uint32_t supported[] = {
        66590, 131072, TEST_SECTORS, 230686720, 243269632, 1953125000,
    };
    struct Fat32Layout layout;
    for (unsigned i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i) {
        assert(fat32_plan(supported[i], &layout));
        assert(layout.clusters >= 65525 && layout.clusters <= 0x0fffffee);
        assert(layout.fat_sectors * 128 >= layout.clusters + 2);
        assert(layout.root_sector + (uint64_t)layout.clusters * layout.sectors_per_cluster <= supported[i]);
    }
    assert(!fat32_plan(66500, &layout));

    struct FormatSink sink = {.stop_at = 200};
    assert(fat32_plan(TEST_SECTORS, &sink.layout));
    assert(fat32_initialize(0, TEST_SECTORS, 1, inspect_sector, &sink) == FAT32_INIT_INVALID_LAYOUT);
    assert(fat32_initialize(UINT32_MAX - 1, TEST_SECTORS, 1, inspect_sector, &sink) == FAT32_INIT_INVALID_LAYOUT);
    assert(!sink.writes);
    assert(fat32_initialize(TEST_START, TEST_SECTORS, 1, inspect_sector, &sink) == FAT32_INIT_WRITE_FAILED);
    assert(sink.writes == 201 && !sink.published);

    struct FormatSink successful = {.layout = sink.layout, .stop_at = UINT32_MAX};
    if (argc > 1) {
        successful.journal = fopen(argv[1], "wb");
        assert(successful.journal);
    }
    assert(fat32_initialize(TEST_START, TEST_SECTORS, 0x50585031, inspect_sector, &successful) == FAT32_INIT_OK);
    assert(successful.last == 0 && successful.published == 1);
    assert(successful.writes == sink.layout.root_sector + sink.layout.sectors_per_cluster + 2);
    assert(le32(successful.boot + 28) == TEST_START);
    assert(le32(successful.boot + 32) == TEST_SECTORS);
    assert(le32(successful.boot + 36) == sink.layout.fat_sectors);
    assert(le32(successful.boot + 44) == 2);
    if (successful.journal)
        assert(fclose(successful.journal) == 0);

    struct Fat32Ram ram = {.start = TEST_START, .length = TEST_SECTORS};
    uint8_t sector[FAT32_SECTOR_BYTES];
    assert(fat32_initialize(TEST_START, TEST_SECTORS, 0x50585031, fat32_ram_write, &ram) == FAT32_INIT_OK);
    assert(fat32_ram_verify(&ram));
    assert(!fat32_ram_read(&ram, TEST_SECTORS, sector) && !fat32_ram_write(&ram, TEST_SECTORS, sector));
    assert(fat32_ram_read(&ram, 6, sector));
    sector[510] = 0;
    assert(fat32_ram_write(&ram, 6, sector) && !fat32_ram_verify(&ram));
    puts("PASS: FAT32 geometry through 1 TB, bounded metadata, late boot publication and I/O failure");
}
