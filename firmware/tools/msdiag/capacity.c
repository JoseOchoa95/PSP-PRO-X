/* GPL-3.0-or-later */
#include "capacity.h"
#include "probe.h"

#define CAPACITY_LIMIT_SECTORS (CAPACITY_LIMIT_BYTES / SECTOR_BYTES)

struct CapacityEstimate estimate_capacity(uint32_t reported_sectors,
                                          CapacityReader read_sector, void *context)
{
    struct CapacityEstimate unknown = {CAPACITY_INCONCLUSIVE, 0};
    uint32_t candidate;
    if (!reported_sectors || reported_sectors >= COUNT_WRAP_SECTORS)
        return unknown;

    for (candidate = reported_sectors; candidate < CAPACITY_LIMIT_SECTORS;
         candidate += COUNT_WRAP_SECTORS) {
        enum CapacityReply outside = read_sector(context, candidate);
        if (outside == CAPACITY_READ)
            continue;
        if (outside != CAPACITY_TRANSFER_FAILURE || candidate + 1 >= CAPACITY_LIMIT_SECTORS)
            return unknown;

        /* A transport failure alone cannot identify the end. Require one more
           failure past the candidate and successful reads inside after each. */
        if (read_sector(context, candidate - 1) != CAPACITY_READ ||
            read_sector(context, candidate + 1) != CAPACITY_TRANSFER_FAILURE ||
            read_sector(context, candidate - 1) != CAPACITY_READ)
            return unknown;
        return (struct CapacityEstimate){CAPACITY_CANDIDATE, candidate};
    }

    if (read_sector(context, CAPACITY_LIMIT_SECTORS - 1) == CAPACITY_READ)
        return (struct CapacityEstimate){CAPACITY_CAP_SAMPLE, CAPACITY_LIMIT_SECTORS};
    return unknown;
}
