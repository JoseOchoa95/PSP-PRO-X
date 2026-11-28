/* GPL-3.0-or-later */
#ifndef MSFORMAT_CARD_SNAPSHOT_H
#define MSFORMAT_CARD_SNAPSHOT_H

#include "gpt.h"

enum { MS_FORMAT_WITNESSES = 2 };

struct MsCardSnapshot {
    uint8_t mbr[FAT32_SECTOR_BYTES];
    struct {
        uint32_t lba;
        uint8_t sector[FAT32_SECTOR_BYTES];
    } witnesses[MS_FORMAT_WITNESSES];
};

int ms_card_witness_allowed(const struct MsFormatPlan *plan, uint32_t lba);

/* Capture distinct, nonuniform sectors outside every planned write range.
   The caller must already have checked the usable extent of the insertion. */
int ms_card_capture(const struct MsFormatPlan *plan, const struct MsGptLayout *gpt, uint32_t reported, MsReadSector read,
                    void *device, struct MsCardSnapshot *snapshot);

/* MBR remains part of the identity until the formatter begins rewriting it. */
int ms_card_matches(const struct MsCardSnapshot *snapshot, int include_mbr,
                    MsReadSector read, void *device);

#endif
