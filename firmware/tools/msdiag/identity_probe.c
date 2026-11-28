/* GPL-3.0-or-later */
#include <string.h>

#include "identity_probe.h"

enum { SYSTEM_INFO = 0x10, SERIAL_OFFSET = 20, PRO_ID_OFFSET = 64 };

uint32_t ms_identity_system_info_offset(const uint8_t page[SECTOR_BYTES])
{
    struct MsAttribute attrs[MS_ATTRIBUTE_MAX];
    unsigned count = decode_attribute_directory(page, attrs);
    uint32_t offset = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (attrs[i].id != SYSTEM_INFO)
            continue;
        if (offset || attrs[i].size != MEDIA_ATTRIBUTE_BYTES)
            return 0;
        offset = attrs[i].offset;
    }
    for (unsigned i = 0; offset && i < count; ++i) {
        if (attrs[i].id != SYSTEM_INFO && attrs[i].size && attrs[i].offset < offset + MEDIA_ATTRIBUTE_BYTES &&
            attrs[i].offset + attrs[i].size > offset)
            return 0;
    }
    return offset;
}

static int field_present(const uint8_t *data, unsigned size)
{
    if (data[0] != 0 && data[0] != 0xff)
        return 1;
    for (unsigned i = 1; i < size; ++i) {
        if (data[i] != data[0])
            return 1;
    }
    return 0;
}

int ms_identity_decode(const uint8_t info[MEDIA_ATTRIBUTE_BYTES], struct MsProIdentity *identity)
{
    *identity = (struct MsProIdentity){0};
    if (info[0] != 2)
        return 0;
    memcpy(identity->serial, info + SERIAL_OFFSET, sizeof(identity->serial));
    memcpy(identity->pro_id, info + PRO_ID_OFFSET, sizeof(identity->pro_id));
    if (field_present(identity->serial, sizeof(identity->serial)))
        identity->available |= MS_ID_SERIAL;
    if (field_present(identity->pro_id, sizeof(identity->pro_id)))
        identity->available |= MS_ID_PRO;
    return 1;
}
