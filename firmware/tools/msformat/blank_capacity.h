/* GPL-3.0-or-later */
#ifndef MSFORMAT_BLANK_CAPACITY_H
#define MSFORMAT_BLANK_CAPACITY_H

#include "blank_probe.h"
#include "card_snapshot.h"

struct MsBlankCapacity {
    struct MsBlankProbe probes[MS_FORMAT_WITNESSES];
    struct MsBlankSamples samples[MS_FORMAT_WITNESSES];
};

int ms_blank_capacity_headers_zero(MsReadSector read, void *device);
/* Call only for a read-only exact boundary candidate with no distinct alias samples. */
int ms_blank_capacity_capture(const struct MsFormatPlan *plan, uint32_t reported, MsReadSector read, void *device,
                              struct MsBlankCapacity *check, struct MsCardSnapshot *snapshot);
int ms_blank_capacity_restored(const struct MsBlankCapacity *check, MsReadSector read, void *device);

#endif
