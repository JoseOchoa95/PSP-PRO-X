/* GPL-3.0-or-later */
#ifndef MSDIAG_AUTO_PROBE_H
#define MSDIAG_AUTO_PROBE_H

#include "capacity.h"
#include "probe.h"

struct AutoCapacity {
    struct CapacityEstimate estimate;
    unsigned alias_checks, distinct_checks;
};

typedef enum CapacityReply (*AutoSectorReader)(void *context, uint32_t lba,
                                              uint8_t sector[SECTOR_BYTES]);

/* Every sector is sampled read-only on the current insertion. */
struct AutoCapacity probe_auto_capacity(uint32_t reported, int use_block_count_hint,
                                        AutoSectorReader read, void *context);

#endif
