/* GPL-3.0-or-later */
#ifndef MSDIAG_CAPACITY_H
#define MSDIAG_CAPACITY_H

#include <stdint.h>

#define CAPACITY_LIMIT_BYTES UINT64_C(1000000000000)
#define COUNT_WRAP_SECTORS (UINT32_C(65536) * 4096)

enum CapacityReply {
    CAPACITY_READ,
    CAPACITY_TRANSFER_FAILURE,
    CAPACITY_OTHER_FAILURE,
};

enum CapacityFinding {
    CAPACITY_INCONCLUSIVE,
    CAPACITY_CANDIDATE,
    CAPACITY_CAP_SAMPLE,
};

struct CapacityEstimate {
    enum CapacityFinding finding;
    uint32_t sectors;
};

typedef enum CapacityReply (*CapacityReader)(void *context, uint32_t lba);

/* This probes the 16-bit count hypothesis for 4096-sector allocation blocks. */
struct CapacityEstimate estimate_capacity(uint32_t reported_sectors,
                                          CapacityReader read_sector, void *context);

#endif
