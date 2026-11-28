/* GPL-3.0-or-later */
#include <string.h>

#include "../msdiag/probe.h"
#include "blank_probe.h"

int ms_blank_probe_plan(uint32_t reported, uint32_t end, uint32_t high, struct MsBlankProbe *probe)
{
    if (!probe)
        return 0;
    *probe = (struct MsBlankProbe){0};
    if (!reported || high < reported || high >= end)
        return 0;
    const uint32_t periods[] = {MS_PROBE_FOUR_GIB_SECTORS, DECIMAL_CUTOFF, BINARY_CUTOFF, reported};
    struct MsBlankProbe plan = {.high = high};
    for (unsigned i = 0; i < sizeof(periods) / sizeof(periods[0]); ++i) {
        if (high < periods[i])
            continue;
        uint32_t control = high % periods[i];
        if (control < 2048)
            return 0;
        unsigned j = 0;
        while (j < plan.count && plan.controls[j] != control)
            ++j;
        if (j == plan.count)
            plan.controls[plan.count++] = control;
    }
    if (!plan.count)
        return 0;
    *probe = plan;
    return 1;
}

static const uint8_t zero[FAT32_SECTOR_BYTES] = {0};

int ms_blank_probe_capture(const struct MsBlankProbe *probe, struct MsBlankSamples *samples,
                           MsReadSector read, void *device)
{
    uint8_t sector[FAT32_SECTOR_BYTES];
    if (!probe || !probe->high || !probe->count || probe->count > MS_BLANK_CONTROL_LIMIT || !read || !samples ||
        !read(device, probe->high, sector) || memcmp(sector, zero, sizeof(zero)))
        return 0;
    memset(samples, 0, sizeof(*samples));
    for (unsigned i = 0; i < probe->count; ++i) {
        if (probe->controls[i] < 2048 || probe->controls[i] >= probe->high ||
            !read(device, probe->controls[i], samples->lower[i]))
            return 0;
    }
    return 1;
}

int ms_blank_probe_matches(const struct MsBlankProbe *probe, const struct MsBlankSamples *samples,
                           MsReadSector read, void *device)
{
    struct MsBlankSamples current;
    return samples && ms_blank_probe_capture(probe, &current, read, device) &&
           !memcmp(&current, samples, sizeof(current));
}

static int read_pattern(MsReadSector read, void *device, uint32_t lba,
                        const uint8_t *pattern, const uint8_t *original)
{
    uint8_t sector[FAT32_SECTOR_BYTES];
    if (!read(device, lba, sector))
        return -1;
    if (!memcmp(sector, original, sizeof(sector)))
        return 0;
    return !memcmp(sector, pattern, sizeof(sector)) ? 1 : -1;
}

struct MsBlankResult ms_blank_probe_run(const struct MsBlankProbe *probe, const struct MsBlankSamples *samples,
                                       MsReadSector read,
                                       Fat32WriteSector write, int (*same_card)(void *), void *device)
{
    struct MsBlankResult result = {0};
    uint8_t pattern[FAT32_SECTOR_BYTES];
    if (!write || !same_card || !same_card(device) || !ms_blank_probe_matches(probe, samples, read, device) ||
        !same_card(device))
        return result;
    for (unsigned i = 0; i < sizeof(pattern); ++i)
        pattern[i] = (probe->high >> (8 * (i % 4))) ^ (0xa5u + i);
    for (unsigned i = 0; i < probe->count; ++i) {
        if (!memcmp(samples->lower[i], pattern, sizeof(pattern)))
            return result;
    }

    result.finding = MS_BLANK_RESTORE_UNVERIFIED;
    ++result.writes;
    int accepted = write(device, probe->high, pattern);
    if (!same_card(device))
        return result;
    int high = read_pattern(read, device, probe->high, pattern, zero);
    if (high < 0)
        return result;
    const uint8_t *restore = high ? zero : NULL;
    for (unsigned i = 0; i < probe->count; ++i) {
        int low = read_pattern(read, device, probe->controls[i], pattern, samples->lower[i]);
        if (low < 0)
            return result;
        if (!low)
            continue;
        if (!result.alias_lba)
            result.alias_lba = probe->controls[i];
        if (restore && memcmp(restore, samples->lower[i], sizeof(zero)))
            return result;
        restore = samples->lower[i];
    }
    if (restore) {
        /* All sectors changed to our pattern must agree on their original bytes
           before one write to the same address can attempt to restore them. */
        if (!same_card(device))
            return result;
        ++result.writes;
        write(device, probe->high, restore);
    }
    if (!ms_blank_probe_matches(probe, samples, read, device) || !same_card(device))
        return result;
    if (result.alias_lba)
        result.finding = MS_BLANK_ALIAS_RESTORED;
    else
        result.finding = high && accepted ? MS_BLANK_INDEPENDENT : MS_BLANK_WRITE_REJECTED;
    return result;
}
