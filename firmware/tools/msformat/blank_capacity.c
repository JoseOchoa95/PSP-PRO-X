/* GPL-3.0-or-later */
#include <string.h>

#include "blank_capacity.h"

static const uint8_t zero[FAT32_SECTOR_BYTES] = {0};

int ms_blank_capacity_headers_zero(MsReadSector read, void *device)
{
    const uint32_t headers[] = {0, 1, 2048};
    uint8_t sector[FAT32_SECTOR_BYTES];
    for (unsigned i = 0; i < sizeof(headers) / sizeof(headers[0]); ++i) {
        if (!read(device, headers[i], sector) || memcmp(sector, zero, sizeof(sector)))
            return 0;
    }
    return 1;
}

static int capture_probe(const struct MsFormatPlan *plan, uint32_t reported, uint32_t high,
                         MsReadSector read, void *device, struct MsBlankProbe *probe, struct MsBlankSamples *samples)
{
    if (!ms_card_witness_allowed(plan, high) || !ms_blank_probe_plan(reported, plan->usable_sectors, high, probe))
        return 0;
    for (unsigned i = 0; i < probe->count; ++i) {
        if (probe->controls[i] <= 2048)
            return 0;
    }
    if (!ms_blank_probe_capture(probe, samples, read, device))
        return 0;
    for (unsigned i = 0; i < probe->count; ++i) {
        if (memcmp(samples->lower[i], zero, sizeof(zero)))
            return 0;
    }
    return 1;
}

int ms_blank_capacity_restored(const struct MsBlankCapacity *check, MsReadSector read, void *device)
{
    if (!ms_blank_capacity_headers_zero(read, device))
        return 0;
    for (unsigned i = 0; i < MS_FORMAT_WITNESSES; ++i) {
        if (!ms_blank_probe_matches(&check->probes[i], &check->samples[i], read, device))
            return 0;
    }
    return 1;
}

int ms_blank_capacity_capture(const struct MsFormatPlan *plan, uint32_t reported, MsReadSector read, void *device,
                              struct MsBlankCapacity *check, struct MsCardSnapshot *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    if (reported <= 2048 || reported >= plan->usable_sectors || !ms_blank_capacity_headers_zero(read, device))
        return 0;
    uint32_t end = plan->usable_sectors;
    uint32_t last = end - 1;
    if (!capture_probe(plan, reported, last, read, device, &check->probes[1], &check->samples[1]))
        return 0;

    const uint32_t bases[] = {reported + (end - reported) / 2, end / 4 * 3, reported};
    const uint32_t offsets[] = {0, 2048, 4096, 32768, 65536};
    for (unsigned i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        for (unsigned j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j) {
            uint64_t high = (uint64_t)bases[i] + offsets[j];
            if (high >= last ||
                !capture_probe(plan, reported, (uint32_t)high, read, device, &check->probes[0], &check->samples[0]))
                continue;
            if (!ms_blank_capacity_restored(check, read, device))
                return 0;
            snapshot->witnesses[0].lba = (uint32_t)high;
            snapshot->witnesses[1].lba = last;
            return 1;
        }
    }
    return 0;
}
