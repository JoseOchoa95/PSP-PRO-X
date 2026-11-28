/* GPL-3.0-or-later */
#ifndef MSDIAG_IDENTITY_PROBE_H
#define MSDIAG_IDENTITY_PROBE_H

#include "probe.h"

enum { MS_ID_SERIAL = 1, MS_ID_PRO = 2 };

struct MsProIdentity {
    uint8_t serial[4], pro_id[16];
    unsigned available;
};

uint32_t ms_identity_system_info_offset(const uint8_t page[SECTOR_BYTES]);
int ms_identity_decode(const uint8_t info[MEDIA_ATTRIBUTE_BYTES], struct MsProIdentity *identity);

#endif
