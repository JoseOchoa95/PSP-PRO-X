/* GPL-3.0-or-later */
#ifndef MSSWITCH_CARD_MOUNT_H
#define MSSWITCH_CARD_MOUNT_H

#include "layout.h"

struct StockReader;

int ms_mount_first(const struct StockReader *reader, const struct MsFormatVolume *first, uint32_t volume_id);
void ms_mount_report(void);

#endif
