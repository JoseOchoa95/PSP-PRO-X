/* GPL-3.0-or-later */
#ifndef MSFORMAT_GPT_H
#define MSFORMAT_GPT_H

#include "layout.h"

struct MsGptLayout {
    uint32_t sectors_per_copy, backup_start;
    uint32_t primary_crc, backup_crc;
};

static inline int ms_gpt_contains(const struct MsGptLayout *gpt, uint32_t lba)
{
    return gpt && ((lba && lba <= gpt->sectors_per_copy) ||
           (lba >= gpt->backup_start && lba - gpt->backup_start < gpt->sectors_per_copy));
}

/* Requires an exact device boundary and a safe, already planned MBR replacement. */
int ms_gpt_inspect(const struct MsFormatPlan *plan, MsReadSector read, void *device, struct MsGptLayout *gpt);

#endif
