/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fat32_dirs.h"

enum { START = 461375488, LENGTH = 39319552, ENTRY_BYTES = 32 };

static struct Fat32Ram volume;

static void put32(unsigned char *data, uint32_t value)
{
    for (unsigned byte = 0; byte < 4; ++byte)
        data[byte] = (uint8_t)(value >> (byte * 8));
}

static void add_entry(unsigned char *entry, const char *name, unsigned cluster)
{
    assert(strlen(name) <= 11 && cluster < 256);
    memset(entry, ' ', 11);
    memcpy(entry, name, strlen(name));
    entry[11] = 0x10;
    entry[26] = (uint8_t)cluster;
}

static void fixture(void)
{
    static const char *const short_names[] = {
        "PSP", "GAME", "SAVEDATA", "COMMON", "SYSTEM", "MP_ROOT",
        "100MNV01", "101ANV01", "MUSIC", "PICTURE", "VIDEO", "THEME", "RSSCH", "IMPORT",
        "SEPLUG~1", "ISO", "VIDEO",
    };
    static const unsigned parents[] = {0, 1, 1, 1, 1, 0, 6, 6, 0, 0, 0, 1, 1, 13, 0, 0, 16};
    struct Fat32Layout layout;
    assert(fat32_plan(LENGTH, &layout));
    unsigned char fat[FAT32_SECTOR_BYTES] = {0};
    assert(fat32_ram_read(&volume, 32, fat));
    for (unsigned i = 0; i < PSP_INITIAL_DIRECTORY_COUNT; ++i)
        put32(fat + (i + 3) * 4, 0x0fffffff);
    assert(fat32_ram_write(&volume, 32, fat));
    assert(fat32_ram_write(&volume, 32 + layout.fat_sectors, fat));
    for (unsigned parent = 0; parent <= PSP_INITIAL_DIRECTORY_COUNT; ++parent) {
        unsigned char data[FAT32_SECTOR_BYTES] = {0};
        unsigned offset = 0;
        if (parent) {
            add_entry(data, ".", parent + 2);
            unsigned grandparent = parents[parent - 1];
            add_entry(data + ENTRY_BYTES, "..", grandparent ? grandparent + 2 : 0);
            offset = 2 * ENTRY_BYTES;
        }
        for (unsigned child = 0; child < PSP_INITIAL_DIRECTORY_COUNT; ++child) {
            if (parents[child] == parent) {
                if (!strcmp(short_names[child], "SEPLUG~1")) {
                    static const unsigned char long_name[ENTRY_BYTES] =
                        "\x41\x53\x00\x45\x00\x50\x00\x4c\x00\x55\x00\x0f\x00\xa3\x47\x00"
                        "\x49\x00\x4e\x00\x53\x00\x00\x00\xff\xff\x00\x00\xff\xff\xff\xff";
                    memcpy(data + offset, long_name, ENTRY_BYTES);
                    offset += ENTRY_BYTES;
                }
                add_entry(data + offset, short_names[child], child + 3);
                offset += ENTRY_BYTES;
            }
        }
        assert(fat32_ram_write(&volume, layout.root_sector + parent * layout.sectors_per_cluster, data));
    }
}

static void verify_finish(void)
{
    assert(fat32_ram_finish_directories(&volume));
    unsigned char info[FAT32_SECTOR_BYTES];
    unsigned char backup[FAT32_SECTOR_BYTES];
    assert(fat32_ram_read(&volume, 1, info) && fat32_ram_read(&volume, 7, backup));
    assert(!memcmp(info, backup, sizeof(info)));
    const unsigned char expected[] = {0x37, 0x5f, 0x09, 0, 20, 0, 0, 0};
    assert(!memcmp(info + 488, expected, sizeof(expected)));
}

static void recorded_image(const char *path)
{
    FILE *stream = fopen(path, "rb");
    assert(stream);
    uint32_t length, lba;
    unsigned char sector[FAT32_SECTOR_BYTES];
    memset(&volume, 0, sizeof(volume));
    volume.start = START;
    volume.length = LENGTH;
    assert(fread(&length, sizeof(length), 1, stream) == 1 && length == LENGTH);
    while (fread(&lba, sizeof(lba), 1, stream) == 1) {
        assert(fread(sector, sizeof(sector), 1, stream) == 1);
        assert(fat32_ram_write(&volume, lba, sector));
    }
    assert(feof(stream) && fclose(stream) == 0);
    assert(fat32_ram_add_indicators(&volume));
    verify_finish();
    puts("PASS: C trial verified the actual Sony mkdir replay image");
}

int main(int argc, char **argv)
{
    volume.start = START;
    volume.length = LENGTH;
    assert(fat32_initialize(START, LENGTH, 0x50585031, fat32_ram_write, &volume) == FAT32_INIT_OK);
    assert(fat32_ram_verify(&volume) && !fat32_ram_finish_directories(&volume));
    fixture();
    struct Fat32Layout layout;
    assert(fat32_plan(LENGTH, &layout));
    unsigned char sector[FAT32_SECTOR_BYTES];
    assert(!fat32_ram_finish_directories(&volume));
    assert(fat32_ram_add_indicators(&volume));
    assert(!fat32_ram_add_indicators(&volume));
    assert(fat32_ram_read(&volume, layout.root_sector, sector));
    assert(!memcmp(sector + 8 * ENTRY_BYTES, "MEMSTICKIND\003", 12));
    assert(!memcmp(sector + 9 * ENTRY_BYTES, "MSTK_PROIND\003", 12));
    sector[8 * ENTRY_BYTES + 28] = 1;
    assert(fat32_ram_write(&volume, layout.root_sector, sector));
    assert(!fat32_ram_finish_directories(&volume));
    sector[8 * ENTRY_BYTES + 28] = 0;
    sector[5 * ENTRY_BYTES + 13] ^= 1;
    assert(fat32_ram_write(&volume, layout.root_sector, sector));
    assert(!fat32_ram_finish_directories(&volume));
    sector[5 * ENTRY_BYTES + 13] ^= 1;
    assert(fat32_ram_write(&volume, layout.root_sector, sector));
    verify_finish();
    assert(fat32_ram_read(&volume, 32 + layout.fat_sectors, sector));
    sector[12] = 0;
    assert(fat32_ram_write(&volume, 32 + layout.fat_sectors, sector));
    unsigned writes = volume.writes;
    assert(!fat32_ram_finish_directories(&volume) && volume.writes == writes);
    assert(fat32_ram_read(&volume, 32, sector));
    assert(fat32_ram_write(&volume, 32 + layout.fat_sectors, sector));
    assert(fat32_ram_read(&volume, layout.root_sector, sector));
    sector[0] = 'X';
    assert(fat32_ram_write(&volume, layout.root_sector, sector));
    writes = volume.writes;
    assert(!fat32_ram_finish_directories(&volume) && volume.writes == writes);
    puts("PASS: RAM directories, FSInfo, bad tree and mismatched FAT refusal");
    if (argc == 2)
        recorded_image(argv[1]);
    return 0;
}
