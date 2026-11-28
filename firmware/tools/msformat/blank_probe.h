/* GPL-3.0-or-later */
#ifndef MSFORMAT_BLANK_PROBE_H
#define MSFORMAT_BLANK_PROBE_H

#include "write_probe.h"

enum { MS_BLANK_CONTROL_LIMIT = 4 };

struct MsBlankProbe {
    uint32_t high;
    uint32_t controls[MS_BLANK_CONTROL_LIMIT];
    unsigned count;
};

struct MsBlankSamples {
    uint8_t lower[MS_BLANK_CONTROL_LIMIT][FAT32_SECTOR_BYTES];
};

enum MsBlankFinding {
    MS_BLANK_REFUSED,
    MS_BLANK_INDEPENDENT,
    MS_BLANK_ALIAS_RESTORED,
    MS_BLANK_WRITE_REJECTED,
    MS_BLANK_RESTORE_UNVERIFIED,
};

struct MsBlankResult {
    enum MsBlankFinding finding;
    uint32_t alias_lba;
    unsigned writes;
};

int ms_blank_probe_plan(uint32_t reported, uint32_t end, uint32_t high, struct MsBlankProbe *probe);
int ms_blank_probe_capture(const struct MsBlankProbe *probe, struct MsBlankSamples *samples,
                           MsReadSector read, void *device);
int ms_blank_probe_matches(const struct MsBlankProbe *probe, const struct MsBlankSamples *samples,
                           MsReadSector read, void *device);
/* The device owns the I/O lock. The write callback must restrict all writes to
   the approved high LBA and independently recheck the insertion. */
struct MsBlankResult ms_blank_probe_run(const struct MsBlankProbe *probe, const struct MsBlankSamples *samples,
                                       MsReadSector read,
                                       Fat32WriteSector write, int (*same_card)(void *), void *device);

#endif
