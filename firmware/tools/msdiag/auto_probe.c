/* GPL-3.0-or-later */
#include <string.h>

#include "auto_probe.h"

enum { CAP_SECTORS = CAPACITY_LIMIT_BYTES / SECTOR_BYTES, FOUR_GIB = 8388608 };

struct SectorProbe {
    AutoSectorReader read;
    void *context;
    uint8_t sector[SECTOR_BYTES];
};

static enum CapacityReply read_at(void *context, uint32_t lba)
{
    struct SectorProbe *probe = context;
    return probe->read(probe->context, lba, probe->sector);
}

static struct CapacityEstimate verify_boundary(struct SectorProbe *probe, uint32_t first_outside)
{
    static const struct CapacityEstimate unknown = {CAPACITY_INCONCLUSIVE, 0};
    if (!first_outside || first_outside + 1 >= CAP_SECTORS ||
        read_at(probe, first_outside - 1) != CAPACITY_READ ||
        read_at(probe, first_outside) != CAPACITY_TRANSFER_FAILURE ||
        read_at(probe, first_outside - 1) != CAPACITY_READ ||
        read_at(probe, first_outside + 1) != CAPACITY_TRANSFER_FAILURE ||
        read_at(probe, first_outside - 1) != CAPACITY_READ)
        return unknown;
    return (struct CapacityEstimate){CAPACITY_CANDIDATE, first_outside};
}

static struct CapacityEstimate scan_boundary(struct SectorProbe *probe, uint32_t reported)
{
    static const struct CapacityEstimate unknown = {CAPACITY_INCONCLUSIVE, 0};
    uint32_t low = reported < CAP_SECTORS ? reported - 1 : CAP_SECTORS - 1;
    if (read_at(probe, low) != CAPACITY_READ)
        return unknown;
    if (low == CAP_SECTORS - 1)
        return (struct CapacityEstimate){CAPACITY_CAP_SAMPLE, CAP_SECTORS};

    uint32_t high = low + 1;
    for (;;) {
        enum CapacityReply status = read_at(probe, high);
        if (status == CAPACITY_TRANSFER_FAILURE)
            break;
        if (status != CAPACITY_READ)
            return unknown;
        if (high == CAP_SECTORS - 1)
            return (struct CapacityEstimate){CAPACITY_CAP_SAMPLE, CAP_SECTORS};
        low = high;
        uint64_t next = (uint64_t)high * 2;
        high = next < CAP_SECTORS ? (uint32_t)next : CAP_SECTORS - 1;
    }
    while (high - low > 1) {
        uint32_t middle = low + (high - low) / 2;
        enum CapacityReply status = read_at(probe, middle);
        if (status == CAPACITY_READ)
            low = middle;
        else if (status == CAPACITY_TRANSFER_FAILURE)
            high = middle;
        else
            return unknown;
    }
    return verify_boundary(probe, high);
}

static int compare_alias(struct SectorProbe *probe, uint32_t high, uint32_t low)
{
    uint8_t original[SECTOR_BYTES];
    if (read_at(probe, high) != CAPACITY_READ)
        return -1;
    memcpy(original, probe->sector, sizeof(original));
    if (read_at(probe, low) != CAPACITY_READ)
        return -1;
    return memcmp(original, probe->sector, sizeof(original)) != 0;
}

static int check_period(struct SectorProbe *probe, uint32_t end, uint32_t period)
{
    uint32_t high_base = (end - 1) / period * period;
    const uint32_t offsets[] = {0, 1, 2048, 4096, 32768, (end - 1) % period};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        if (offsets[i] >= period || offsets[i] >= end - high_base)
            continue;
        int result = compare_alias(probe, high_base + offsets[i], offsets[i]);
        if (result)
            return result;
    }
    return 0;
}

struct AutoCapacity probe_auto_capacity(uint32_t reported, int use_block_count_hint,
                                        AutoSectorReader read, void *context)
{
    struct AutoCapacity result = {{CAPACITY_INCONCLUSIVE, 0}, 0, 0};
    if (!reported || !read)
        return result;
    struct SectorProbe probe = {.read = read, .context = context};
    if (use_block_count_hint)
        result.estimate = estimate_capacity(reported, read_at, &probe);
    if (result.estimate.finding == CAPACITY_INCONCLUSIVE)
        result.estimate = scan_boundary(&probe, reported);
    if (result.estimate.finding == CAPACITY_INCONCLUSIVE || result.estimate.sectors <= DECIMAL_CUTOFF)
        return result;

    const uint32_t periods[] = {FOUR_GIB, DECIMAL_CUTOFF, BINARY_CUTOFF, reported};
    for (unsigned i = 0; i < sizeof(periods) / sizeof(periods[0]); ++i) {
        uint32_t period = periods[i];
        if (period >= result.estimate.sectors ||
            (i == 3 && (period == FOUR_GIB || period == DECIMAL_CUTOFF || period == BINARY_CUTOFF)))
            continue;
        int comparison = check_period(&probe, result.estimate.sectors, period);
        if (comparison < 0)
            return (struct AutoCapacity){{CAPACITY_INCONCLUSIVE, 0}, 0, 0};
        ++result.alias_checks;
        result.distinct_checks += (unsigned)comparison;
    }
    return result;
}
