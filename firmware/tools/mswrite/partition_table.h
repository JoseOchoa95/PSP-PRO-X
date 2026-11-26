/* GPL-3.0-or-later */
#ifndef MSPART_PARTITION_TABLE_H
#define MSPART_PARTITION_TABLE_H

#include <stdint.h>

#define TRIAL_PARTITIONS 3

struct TrialPartition {
    uint32_t start, count;
};

/* Create the bounded primary MBR for the verified 256 GB trial card. */
int build_trial_mbr(const unsigned char *original, unsigned char *result,
                    struct TrialPartition layout[TRIAL_PARTITIONS]);

#endif
