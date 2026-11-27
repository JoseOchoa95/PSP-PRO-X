/* GPL-3.0-or-later */
#ifndef MSDIAG_PSP_MEDIA_H
#define MSDIAG_PSP_MEDIA_H

#include <psptypes.h>

#include "auto_probe.h"
#include "card_snapshot.h"
#include "stock_reader.h"

struct MsAutoMedia {
    struct StockReader reader;
    struct AutoCapacity capacity;
    struct MsFormatPlan plan;
    struct MsCardSnapshot snapshot;
    enum MsFormatLayoutResult layout;
    uint32_t reported;
    int gpt, have_snapshot;
};

const char *ms_auto_profile(SceUID file, struct StockReader *reader, uint32_t *reported, int *block_count_hint);
const char *ms_auto_measure(SceUID file, struct MsAutoMedia *media);
int ms_auto_read_sector(void *reader, uint32_t lba, uint8_t *out);
int ms_auto_check_boundary(struct StockReader *reader, uint32_t end);

#endif
