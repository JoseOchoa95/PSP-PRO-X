/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "blank_probe.h"

enum { ADAPTER = 232259584, SECTORS = 500695040, HIGH = 269484032 };
enum Mode {
    INDEPENDENT, ALIAS, REJECTED, REPORTED_FAILURE, NO_RESTORE, SWAPPED, CHANGED_CONTROL, NONZERO,
    MIXED_CONTROL, ASYMMETRIC_ALIAS, CONFLICT, CHANGED_AFTER_PREVIEW,
};

struct Medium {
    struct MsBlankProbe probe;
    enum Mode mode;
    unsigned writes;
    uint8_t sectors[1 + MS_BLANK_CONTROL_LIMIT][FAT32_SECTOR_BYTES];
};

static int present(void *context)
{
    const struct Medium *medium = context;
    return medium->mode != SWAPPED || !medium->writes;
}

static int read_sector(void *context, uint32_t lba, uint8_t *out)
{
    const struct Medium *medium = context;
    unsigned slot = 0;
    if (lba == medium->probe.high) {
        slot = medium->mode == ALIAS ? 1 : 0;
    } else {
        while (slot < medium->probe.count && medium->probe.controls[slot] != lba)
            ++slot;
        assert(slot < medium->probe.count);
        ++slot;
    }
    memcpy(out, medium->sectors[slot], FAT32_SECTOR_BYTES);
    return 1;
}

static int write_sector(void *context, uint32_t lba, const uint8_t *data)
{
    struct Medium *medium = context;
    assert(lba == medium->probe.high && present(context));
    ++medium->writes;
    if (medium->mode == REJECTED || (medium->mode == NO_RESTORE && medium->writes == 2))
        return medium->mode != REJECTED;
    unsigned slot = medium->mode == ALIAS || medium->mode == ASYMMETRIC_ALIAS ? 1 : 0;
    memcpy(medium->sectors[slot], data, FAT32_SECTOR_BYTES);
    if (medium->mode == CONFLICT)
        memcpy(medium->sectors[1], data, FAT32_SECTOR_BYTES);
    if (medium->mode == CHANGED_CONTROL)
        medium->sectors[1][125] = 1;
    return medium->mode != REPORTED_FAILURE;
}

static void check(enum Mode mode, enum MsBlankFinding finding, unsigned writes, int restored)
{
    struct Medium medium = {.mode = mode};
    struct MsBlankSamples samples = {0};
    assert(ms_blank_probe_plan(ADAPTER, SECTORS, HIGH, &medium.probe));
    if (mode == NONZERO)
        medium.sectors[0][15] = 4;
    if (mode == MIXED_CONTROL || mode == ASYMMETRIC_ALIAS || mode == CONFLICT)
        medium.sectors[1][15] = 4;
    assert(ms_blank_probe_capture(&medium.probe, &samples, read_sector, &medium) == (mode != NONZERO));
    if (mode == CHANGED_AFTER_PREVIEW)
        medium.sectors[1][15] = 4;
    struct MsBlankResult result = ms_blank_probe_run(&medium.probe, &samples, read_sector, write_sector,
                                                     present, &medium);
    assert(result.finding == finding && result.writes == writes && medium.writes == writes);
    assert(ms_blank_probe_matches(&medium.probe, &samples, read_sector, &medium) == restored);
    int alias = mode == ALIAS || mode == ASYMMETRIC_ALIAS || mode == CONFLICT;
    assert(result.alias_lba == (alias ? medium.probe.controls[0] : 0));
}

int main(void)
{
    struct MsBlankProbe probe;
    assert(!ms_blank_probe_plan(ADAPTER, SECTORS, ADAPTER - 1, &probe));
    assert(!ms_blank_probe_plan(ADAPTER, SECTORS, ADAPTER, &probe));
    assert(!ms_blank_probe_plan(ADAPTER, SECTORS, SECTORS, &probe));
    assert(ms_blank_probe_plan(ADAPTER, SECTORS, HIGH, &probe) && probe.count == 3);
    check(INDEPENDENT, MS_BLANK_INDEPENDENT, 2, 1);
    check(ALIAS, MS_BLANK_ALIAS_RESTORED, 2, 1);
    check(REJECTED, MS_BLANK_WRITE_REJECTED, 1, 1);
    check(REPORTED_FAILURE, MS_BLANK_WRITE_REJECTED, 2, 1);
    check(NO_RESTORE, MS_BLANK_RESTORE_UNVERIFIED, 2, 0);
    check(SWAPPED, MS_BLANK_RESTORE_UNVERIFIED, 1, 0);
    check(CHANGED_CONTROL, MS_BLANK_RESTORE_UNVERIFIED, 1, 0);
    check(NONZERO, MS_BLANK_REFUSED, 0, 0);
    check(MIXED_CONTROL, MS_BLANK_INDEPENDENT, 2, 1);
    check(ASYMMETRIC_ALIAS, MS_BLANK_ALIAS_RESTORED, 2, 1);
    check(CONFLICT, MS_BLANK_RESTORE_UNVERIFIED, 1, 0);
    check(CHANGED_AFTER_PREVIEW, MS_BLANK_REFUSED, 0, 0);
    puts("PASS: uniform samples, detected alias, restoration, write discard, changed media and bounds");
    return 0;
}
