/* GPL-3.0-or-later */
#ifndef MSFORMAT_WRITE_PROBE_H
#define MSFORMAT_WRITE_PROBE_H

#include "card_snapshot.h"

enum { MS_PROBE_FOUR_GIB_SECTORS = 8388608 };

enum MsWriteReadback {
    MS_PROBE_UNREAD,
    MS_PROBE_ORIGINAL,
    MS_PROBE_PATTERN,
    MS_PROBE_DIFFERENT,
};

struct MsWriteProbeResult {
    int attempted, accepted, restored;
    enum MsWriteReadback readback;
};

/* A second write is only used to restore the original sector. The caller
   validates the target and checks media identity before either write. */
struct MsWriteProbeResult ms_probe_sector(uint32_t lba, MsReadSector read, Fat32WriteSector write,
                                         void *device);
uint32_t ms_probe_high_metadata(const struct MsFormatPlan *plan, uint32_t reported);

#endif
