/* GPL-3.0-or-later */
#ifndef MSDIAG_WRITE_TRIAL_H
#define MSDIAG_WRITE_TRIAL_H

#include "probe.h"

#define TRIAL_SECTORS 500695040u
#define TRIAL_REPORTED_BLOCKS 232259584u
#define TRIAL_PARTITION_END 500692992u
#define TRIAL_HIGH_LBA 500694016u
#ifdef MSPART_TRIAL
#define TRIAL_LBA 0u
#define TRIAL_MAGIC "MSPREF01"
#else
#define TRIAL_LBA TRIAL_HIGH_LBA
#define TRIAL_MAGIC "MSWREF01"
#endif
#define TRIAL_WITNESSES 7

struct WriteTrialReference {
    unsigned char magic[8];
    unsigned char witnesses[TRIAL_WITNESSES][DIGEST_BYTES];
    unsigned char original[SECTOR_BYTES];
    unsigned char marker[SECTOR_BYTES];
};

#endif
