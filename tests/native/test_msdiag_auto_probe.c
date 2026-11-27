/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "auto_probe.h"

enum { LIMIT = CAPACITY_LIMIT_BYTES / SECTOR_BYTES, FOUR_GIB = 8388608 };

struct Medium {
    uint32_t sectors, alias_period, bad_sector, hole;
    unsigned calls;
    int uniform;
};

static enum CapacityReply read_sector(void *context, uint32_t lba, uint8_t *bytes)
{
    struct Medium *medium = context;
    assert(lba < LIMIT);
    ++medium->calls;
    if (lba == medium->bad_sector)
        return CAPACITY_OTHER_FAILURE;
    if (lba >= medium->sectors || lba == medium->hole)
        return CAPACITY_TRANSFER_FAILURE;
    uint32_t actual = medium->alias_period ? lba % medium->alias_period : lba;
    memset(bytes, 0, SECTOR_BYTES);
    if (!medium->uniform)
        memcpy(bytes, &actual, sizeof(actual));
    return CAPACITY_READ;
}

int main(void)
{
    struct Medium medium = {500695040, 0, UINT32_MAX, UINT32_MAX, 0, 0};
    struct AutoCapacity result = probe_auto_capacity(232259584, 1, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CANDIDATE && result.estimate.sectors == medium.sectors);
    assert(result.alias_checks == 4 && result.distinct_checks == 4 && medium.calls == 13);

    medium.sectors = 1048576000;
    result = probe_auto_capacity(243269632, 1, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CANDIDATE && result.estimate.sectors == medium.sectors);
    assert(result.distinct_checks == result.alias_checks);

    medium.sectors = 760123456;
    result = probe_auto_capacity(192000000, 0, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CANDIDATE && result.estimate.sectors == medium.sectors);

    medium.sectors = 67108864;
    result = probe_auto_capacity(medium.sectors, 0, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CANDIDATE && result.alias_checks == 0);

    medium.sectors = 500695040;
    medium.uniform = 1;
    result = probe_auto_capacity(232259584, 1, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CANDIDATE && result.alias_checks == 4 && !result.distinct_checks);
    medium.uniform = 0;
    medium.alias_period = FOUR_GIB;
    result = probe_auto_capacity(232259584, 1, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CANDIDATE && result.distinct_checks < result.alias_checks);

    medium.sectors = LIMIT + 100;
    result = probe_auto_capacity(243269632, 1, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CAP_SAMPLE && result.estimate.sectors == LIMIT &&
           result.distinct_checks < result.alias_checks);
    medium.alias_period = 0;
    result = probe_auto_capacity(LIMIT + 10, 0, read_sector, &medium);
    assert(result.estimate.finding == CAPACITY_CAP_SAMPLE && result.estimate.sectors == LIMIT);

    medium.sectors = 500695040;
    medium.bad_sector = 232259584;
    assert(probe_auto_capacity(232259584, 1, read_sector, &medium).estimate.finding == CAPACITY_INCONCLUSIVE);
    medium.bad_sector = UINT32_MAX;
    medium.hole = 232259584;
    assert(probe_auto_capacity(232259584, 1, read_sector, &medium).estimate.finding == CAPACITY_INCONCLUSIVE);
    assert(probe_auto_capacity(0, 1, read_sector, &medium).estimate.finding == CAPACITY_INCONCLUSIVE);
    puts("PASS: automatic exact boundary candidates, layout cap, unreadable media and alias evidence");
    return 0;
}
