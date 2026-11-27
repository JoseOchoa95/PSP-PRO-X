/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "layout.h"

enum { GIB_SECTORS = 2097152, ALIGN = 2048, CAP = 1953125000 };

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static void check_table(const struct MsFormatPlan *plan)
{
    uint8_t table[512], entry[16];
    struct MsFormatVolume volume;
    assert(ms_format_table_sector(plan, 0, table));
    assert(table[510] == 0x55 && table[511] == 0xaa);
    unsigned count = 0;
    for (; count < plan->primary_count; ++count) {
        memcpy(entry, table + 446 + 16 * count, sizeof(entry));
        assert(entry[4] == 0x0c && entry[0] == (count ? 0 : 0x80));
        assert(ms_format_volume(plan, count, &volume) && !volume.ebr);
        assert(le32(entry + 8) == volume.start && le32(entry + 12) == volume.sectors);
    }
    uint32_t end = volume.start + volume.sectors;
    if (count < plan->volume_count) {
        assert(count == 3 && table[498] == 0x0f);
        uint32_t base = le32(table + 502);
        uint32_t container_end = base + le32(table + 506);
        uint32_t ebr = base;
        for (; ebr; ++count) {
            assert(ms_format_table_sector(plan, ebr, table));
            assert(table[510] == 0x55 && table[511] == 0xaa);
            assert(table[450] == 0x0c && le32(table + 454) == ALIGN);
            assert(ms_format_volume(plan, count, &volume) && volume.ebr == ebr);
            assert(ebr >= end && volume.start == ebr + ALIGN);
            assert(le32(table + 458) == volume.sectors);
            end = volume.start + volume.sectors;
            if (!table[466]) {
                ebr = 0;
            } else {
                assert(table[466] == 0x0f);
                ebr = base + le32(table + 470);
                assert(ebr >= end && container_end == ebr + le32(table + 474));
            }
        }
        assert(end == container_end);
    }
    assert(end <= plan->usable_sectors && count == plan->volume_count);
    assert(!ms_format_table_sector(plan, plan->usable_sectors, table));
}

int main(void)
{
    struct MsFormatPlan plan;
    struct MsFormatVolume part;
    assert(ms_format_layout(0, 1, &plan) == MS_FORMAT_INVALID);
    assert(ms_format_layout(500695041, 500695040, &plan) == MS_FORMAT_INVALID);
    assert(ms_format_layout(GIB_SECTORS - 1, 500695040, &plan) == MS_FORMAT_INVALID);
    assert(ms_format_layout(250000000, 250000000, &plan) == MS_FORMAT_STOCK);
    assert(ms_format_layout(232259584, 250000001, &plan) == MS_FORMAT_EXTENDED);

    assert(ms_format_layout(232259584, 500695040, &plan) == MS_FORMAT_EXTENDED);
    assert(plan.primary_count == 3 && plan.volume_count == 3 && plan.full_sectors == 230686720);
    assert(ms_format_volume(&plan, 1, &part) && part.start == 230688768 && part.sectors == 230686720);
    assert(ms_format_volume(&plan, 2, &part) && part.start == 461375488 && part.sectors == 39319552);
    assert(!ms_format_volume(&plan, 3, &part));
    check_table(&plan);

    assert(ms_format_layout(243269632, 500ULL * GIB_SECTORS, &plan) == MS_FORMAT_EXTENDED);
    assert(plan.primary_count == 3 && plan.volume_count == 5 && plan.full_sectors == 116 * GIB_SECTORS);
    assert(ms_format_volume(&plan, 4, &part) && part.sectors == 36 * GIB_SECTORS - 3 * ALIGN);
    check_table(&plan);

    assert(ms_format_layout(232259584, 500ULL * GIB_SECTORS, &plan) == MS_FORMAT_EXTENDED);
    assert(plan.volume_count == 5 && plan.final_sectors == 60 * GIB_SECTORS - 3 * ALIGN);
    check_table(&plan);

    assert(ms_format_layout(232259584, UINT64_C(2500000000), &plan) == MS_FORMAT_EXTENDED);
    assert(plan.usable_sectors == CAP && plan.volume_count == 9);
    check_table(&plan);

    assert(ms_format_layout(UINT64_C(2400000000), UINT64_C(2500000000), &plan) == MS_FORMAT_EXTENDED);
    assert(plan.usable_sectors == CAP && plan.volume_count == 1 && plan.final_sectors == CAP - ALIGN);
    check_table(&plan);

    const uint32_t full = 110 * GIB_SECTORS;
    assert(ms_format_layout(full, ALIGN + 2 * full, &plan) == MS_FORMAT_EXTENDED);
    assert(plan.volume_count == 2 && !plan.final_sectors);
    assert(ms_format_layout(full, ALIGN + 2 * full + 5000, &plan) == MS_FORMAT_EXTENDED);
    assert(plan.volume_count == 2 && !plan.final_sectors);
    assert(ms_format_layout(full, ALIGN + 4 * full + 5000, &plan) == MS_FORMAT_EXTENDED);
    assert(plan.volume_count == 4 && plan.primary_count == 4 && !plan.final_sectors);
    check_table(&plan);

    puts("PASS: stock boundary, 256 GB, 512 GB, 1 TB cap, MBR/EBR chain and remainder geometry");
    return 0;
}
