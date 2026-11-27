/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "write_probe.h"

enum Mode { SUCCESS, DROPPED, REJECTED_AFTER_WRITE, RESTORE_BLOCKED, READ_FAILED };
enum { TARGET = 243271680 };

struct Disk {
    enum Mode mode;
    unsigned writes;
    uint8_t sector[FAT32_SECTOR_BYTES];
};

static int read_sector(void *context, uint32_t lba, uint8_t *out)
{
    struct Disk *disk = context;
    assert(lba == TARGET);
    if (disk->mode == READ_FAILED)
        return 0;
    memcpy(out, disk->sector, sizeof(disk->sector));
    return 1;
}

static int write_sector(void *context, uint32_t lba, const uint8_t *in)
{
    struct Disk *disk = context;
    assert(lba == TARGET);
    ++disk->writes;
    if (disk->mode == DROPPED)
        return 1;
    if (disk->mode == RESTORE_BLOCKED && disk->writes == 2)
        return 0;
    memcpy(disk->sector, in, sizeof(disk->sector));
    return disk->mode != REJECTED_AFTER_WRITE || disk->writes != 1;
}

int main(void)
{
    struct MsFormatPlan plan;
    assert(ms_format_layout(243269632, 1048576000, &plan) == MS_FORMAT_EXTENDED);
    assert(ms_probe_high_metadata(&plan, 243269632) == TARGET);
    assert(ms_format_layout(232259584, 500695040, &plan) == MS_FORMAT_EXTENDED);
    assert(ms_probe_high_metadata(&plan, 232259584) == 461375488);
    assert(ms_probe_high_metadata(&plan, 461375488) == 461375489);
    assert(ms_probe_high_metadata(&plan, plan.usable_sectors) == 0);
    uint8_t original[FAT32_SECTOR_BYTES] = {0xa5, 0x66, 0x11};
    for (enum Mode mode = SUCCESS; mode <= READ_FAILED; ++mode) {
        struct Disk disk = {.mode = mode};
        memcpy(disk.sector, original, sizeof(original));
        struct MsWriteProbeResult result = ms_probe_sector(TARGET, read_sector, write_sector, &disk);
        assert(result.attempted == (mode != READ_FAILED));
        assert(result.accepted == (mode != READ_FAILED && mode != REJECTED_AFTER_WRITE));
        assert(result.readback == (mode == READ_FAILED ? MS_PROBE_UNREAD :
                                  mode == DROPPED ? MS_PROBE_ORIGINAL : MS_PROBE_PATTERN));
        assert(result.restored == (mode != READ_FAILED && mode != RESTORE_BLOCKED));
        assert(disk.writes == (mode == READ_FAILED ? 0u : mode == DROPPED ? 1u : 2u));
        assert((!memcmp(disk.sector, original, sizeof(original))) == (mode != RESTORE_BLOCKED));
    }
    puts("PASS: one-sector probe, silent discard, restoration after rejection, and blocked restore.");
    return 0;
}
