/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "gpt.h"

enum { SECTORS = 969932800, ADAPTER = 164626432, TABLE_SECTORS = 32, BACKUP = SECTORS - 33 };
static struct GptDisk {
    uint8_t mbr[512], primary[512], backup[512], entries[2][TABLE_SECTORS * 512];
    uint32_t unreadable;
} disk;

static void put32(uint8_t *bytes, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = value >> (i * 8);
}

static void sign_header(uint8_t *header)
{
    put32(header + 16, 0);
    put32(header + 16, crc32(0, header, 92));
}

static void make_header(uint8_t *header, uint32_t own, uint32_t alternate, uint32_t table)
{
    memcpy(header, "EFI PART", 8);
    put32(header + 8, 0x10000);
    put32(header + 12, 92);
    put32(header + 24, own);
    put32(header + 32, alternate);
    put32(header + 40, 34);
    put32(header + 48, BACKUP - 1);
    memset(header + 56, 0x83, 16);
    put32(header + 72, table);
    put32(header + 80, 128);
    put32(header + 84, 128);
    put32(header + 88, crc32(0, disk.entries[0], sizeof(disk.entries[0])));
    sign_header(header);
}

static void reset(void)
{
    memset(&disk, 0, sizeof(disk));
    disk.unreadable = UINT32_MAX;
    disk.mbr[450] = 0xee;
    put32(disk.mbr + 454, 1);
    put32(disk.mbr + 458, SECTORS - 1);
    disk.mbr[510] = 0x55;
    disk.mbr[511] = 0xaa;
    disk.entries[0][0] = 0x31;
    put32(disk.entries[0] + 32, 2048);
    put32(disk.entries[0] + 40, BACKUP - 1);
    memcpy(disk.entries[1], disk.entries[0], sizeof(disk.entries[0]));
    make_header(disk.primary, 1, SECTORS - 1, 2);
    make_header(disk.backup, SECTORS - 1, 1, BACKUP);
}

static int read_sector(void *context, uint32_t lba, uint8_t *sector)
{
    struct GptDisk *test = context;
    const uint8_t *source = NULL;
    if (lba == 0)
        source = test->mbr;
    else if (lba == 1)
        source = test->primary;
    else if (lba < 2 + TABLE_SECTORS)
        source = test->entries[0] + (lba - 2) * 512;
    else if (lba == SECTORS - 1)
        source = test->backup;
    else if (lba >= BACKUP && lba < SECTORS - 1)
        source = test->entries[1] + (lba - BACKUP) * 512;
    assert(source);
    if (lba == test->unreadable)
        return 0;
    memcpy(sector, source, 512);
    return 1;
}

static void rejects(const struct MsFormatPlan *plan)
{
    struct MsGptLayout gpt = {33, BACKUP, 17, 19};
    assert(!ms_gpt_inspect(plan, read_sector, &disk, &gpt));
    assert(!gpt.sectors_per_copy);
}

int main(void)
{
    struct MsFormatPlan plan;
    assert(ms_format_layout(ADAPTER, SECTORS, &plan) == MS_FORMAT_EXTENDED);
    struct MsGptLayout gpt;
    reset();
    assert(ms_gpt_inspect(&plan, read_sector, &disk, &gpt));
    assert(gpt.sectors_per_copy == 33 && gpt.backup_start == BACKUP);
    assert(ms_gpt_contains(&gpt, 1) && ms_gpt_contains(&gpt, 33));
    assert(ms_gpt_contains(&gpt, BACKUP) && ms_gpt_contains(&gpt, SECTORS - 1));
    assert(!ms_gpt_contains(&gpt, 0) && !ms_gpt_contains(&gpt, 34));
    assert(!ms_gpt_contains(&gpt, BACKUP - 1) && !ms_gpt_contains(&gpt, SECTORS));

    put32(disk.mbr + 458, UINT32_MAX);
    assert(ms_gpt_inspect(&plan, read_sector, &disk, &gpt));
    put32(disk.mbr + 458, SECTORS - 2);
    rejects(&plan);
    reset();
    disk.mbr[466] = 0x07;
    rejects(&plan);
    reset();
    disk.backup[16] ^= 1;
    rejects(&plan);
    reset();
    disk.entries[0][128] ^= 1;
    rejects(&plan);
    disk.entries[1][128] ^= 1;
    rejects(&plan);
    reset();
    put32(disk.backup + 72, BACKUP - 1);
    sign_header(disk.backup);
    rejects(&plan);
    reset();
    put32(disk.primary + 80, UINT32_MAX);
    put32(disk.backup + 80, UINT32_MAX);
    sign_header(disk.primary);
    sign_header(disk.backup);
    rejects(&plan);
    reset();
    disk.unreadable = BACKUP;
    rejects(&plan);
    puts("PASS: GPT header and array CRCs, protective MBR and exact cleanup bounds");
    return 0;
}
