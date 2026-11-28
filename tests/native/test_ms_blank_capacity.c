/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "blank_capacity.h"

struct Disk {
    uint32_t end, nonzero_lba;
    int nonzero;
};

static int read_sector(void *context, uint32_t lba, uint8_t *sector)
{
    struct Disk *disk = context;
    if (lba >= disk->end)
        return 0;
    memset(sector, 0, FAT32_SECTOR_BYTES);
    if (disk->nonzero && lba == disk->nonzero_lba)
        sector[35] = 0x12;
    return 1;
}

static void zero_media(uint32_t reported, uint32_t end)
{
    struct Disk disk = {.end = end};
    struct MsBlankCapacity check;
    struct MsCardSnapshot snapshot;
    struct MsFormatPlan plan;
    assert(ms_format_layout(reported, end, &plan) == MS_FORMAT_EXTENDED);
    assert(ms_blank_capacity_capture(&plan, reported, read_sector, &disk, &check, &snapshot));
    assert(snapshot.witnesses[0].lba >= reported && snapshot.witnesses[0].lba < end - 1);
    assert(snapshot.witnesses[1].lba == end - 1 && check.probes[1].high == end - 1);
    assert(ms_card_matches(&snapshot, 1, read_sector, &disk));
    assert(ms_blank_capacity_restored(&check, read_sector, &disk));

    disk.nonzero = 1;
    disk.nonzero_lba = check.probes[1].controls[0];
    assert(!ms_blank_capacity_restored(&check, read_sector, &disk));
    assert(!ms_blank_capacity_capture(&plan, reported, read_sector, &disk, &check, &snapshot));
    disk.nonzero_lba = 0;
    assert(!ms_blank_capacity_capture(&plan, reported, read_sector, &disk, &check, &snapshot));
    disk.nonzero_lba = 1;
    assert(!ms_blank_capacity_capture(&plan, reported, read_sector, &disk, &check, &snapshot));
    disk.nonzero_lba = 2048;
    assert(!ms_blank_capacity_capture(&plan, reported, read_sector, &disk, &check, &snapshot));
    disk.nonzero = 0;
    --disk.end;
    assert(!ms_blank_capacity_capture(&plan, reported, read_sector, &disk, &check, &snapshot));
}

int main(void)
{
    zero_media(232259584, 500695040);
    zero_media(164626432, 969932800);

    const uint32_t reported = 232259584;
    struct Disk protected_alias = {.end = reported * 2 + 1};
    struct MsFormatPlan plan;
    struct MsBlankCapacity check;
    struct MsCardSnapshot witness;
    assert(ms_format_layout(reported, protected_alias.end, &plan) == MS_FORMAT_EXTENDED);
    assert(!ms_blank_capacity_capture(&plan, reported, read_sector, &protected_alias, &check, &witness));
    puts("PASS: zero-sector admission, exact endpoint, protected aliases and original controls");
    return 0;
}
