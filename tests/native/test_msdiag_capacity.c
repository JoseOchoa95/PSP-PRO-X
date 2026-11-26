/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>

#include "capacity.h"
#include "probe.h"

struct Media {
    uint32_t size, fault, hole;
    unsigned int reads;
    int wraps;
};

static enum CapacityReply read_media(void *context, uint32_t lba)
{
    struct Media *media = context;
    assert(lba < CAPACITY_LIMIT_BYTES / SECTOR_BYTES);
    ++media->reads;
    if (lba == media->fault)
        return CAPACITY_OTHER_FAILURE;
    if (lba == media->hole)
        return CAPACITY_TRANSFER_FAILURE;
    return media->wraps || lba < media->size ? CAPACITY_READ : CAPACITY_TRANSFER_FAILURE;
}

int main(void)
{
    struct Media media = {0, UINT32_MAX, UINT32_MAX, 0, 0};
    struct CapacityEstimate estimate;
    const uint32_t limit = CAPACITY_LIMIT_BYTES / SECTOR_BYTES;

    media.size = 500695040;
    estimate = estimate_capacity(232259584, read_media, &media);
    assert(estimate.finding == CAPACITY_CANDIDATE && estimate.sectors == media.size);
    assert(media.reads == 5);

    media.size = 1048576000;
    media.reads = 0;
    estimate = estimate_capacity(243269632, read_media, &media);
    assert(estimate.finding == CAPACITY_CANDIDATE && estimate.sectors == media.size);
    assert(media.reads == 7);

    media.size = 67108864;
    estimate = estimate_capacity(media.size, read_media, &media);
    assert(estimate.finding == CAPACITY_CANDIDATE && estimate.sectors == media.size);

    media.size = 500695040;
    media.fault = 232259584;
    assert(estimate_capacity(232259584, read_media, &media).finding == CAPACITY_INCONCLUSIVE);
    media.fault = UINT32_MAX;
    media.hole = 232259584;
    assert(estimate_capacity(232259584, read_media, &media).finding == CAPACITY_INCONCLUSIVE);
    media.hole = UINT32_MAX;

    /* A different geometry that ends between two candidates stays unknown. */
    assert(estimate_capacity(232259585, read_media, &media).finding == CAPACITY_INCONCLUSIVE);
    assert(estimate_capacity(0, read_media, &media).finding == CAPACITY_INCONCLUSIVE);
    assert(estimate_capacity(COUNT_WRAP_SECTORS, read_media, &media).finding == CAPACITY_INCONCLUSIVE);

    /* A wrapping transport can return bytes at the cap. Do not report a size. */
    media.wraps = 1;
    estimate = estimate_capacity(232259584, read_media, &media);
    assert(estimate.finding == CAPACITY_CAP_SAMPLE);
    media.wraps = 0;
    media.size = limit + COUNT_WRAP_SECTORS;
    estimate = estimate_capacity(232259584, read_media, &media);
    assert(estimate.finding == CAPACITY_CAP_SAMPLE && estimate.sectors == limit);

    puts("PASS: MSDIAG candidate sizing, ambiguous responses and 1 TB cap");
    return 0;
}
