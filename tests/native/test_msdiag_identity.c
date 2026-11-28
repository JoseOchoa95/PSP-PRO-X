/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity_probe.h"

static void store_be32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        out[i] = value >> (24 - 8 * i);
}

static void entry(uint8_t *page, unsigned index, uint8_t id, uint32_t offset, uint32_t size)
{
    uint8_t *record = page + 16 + 12 * index;
    store_be32(record, offset);
    store_be32(record + 4, size);
    record[8] = id;
}

int main(void)
{
    uint8_t page[SECTOR_BYTES] = {0xa5, 0xc3, 0, 0, 2};
    entry(page, 0, 0x10, 416, MEDIA_ATTRIBUTE_BYTES);
    entry(page, 1, 0x15, 704, 48);
    assert(ms_identity_system_info_offset(page) == 416);
    entry(page, 0, 0x10, 500, MEDIA_ATTRIBUTE_BYTES);
    assert(ms_identity_system_info_offset(page) == 500);
    entry(page, 1, 0x10, 704, MEDIA_ATTRIBUTE_BYTES);
    assert(!ms_identity_system_info_offset(page));
    entry(page, 1, 0x15, 502, 48);
    assert(!ms_identity_system_info_offset(page));
    entry(page, 1, 0x15, 704, 48);
    entry(page, 0, 0x10, 416, 95);
    assert(!ms_identity_system_info_offset(page));
    entry(page, 0, 0x10, 32767, MEDIA_ATTRIBUTE_BYTES);
    assert(!ms_identity_system_info_offset(page));
    entry(page, 0, 0x20, 416, MEDIA_ATTRIBUTE_BYTES);
    assert(!ms_identity_system_info_offset(page));

    uint8_t info[MEDIA_ATTRIBUTE_BYTES] = {2};
    struct MsProIdentity first, second;
    memset(info + 64, 0xff, 16);
    assert(ms_identity_decode(info, &first) && !first.available);
    info[22] = 0x30;
    assert(ms_identity_decode(info, &first) && first.available == MS_ID_SERIAL);
    info[69] = 0x80;
    assert(ms_identity_decode(info, &first) && first.available == (MS_ID_SERIAL | MS_ID_PRO));
    assert(first.serial[2] == 0x30 && first.pro_id[5] == 0x80);
    info[6] = 0xef;
    info[39] = 0x88;
    assert(ms_identity_decode(info, &second));
    assert(!memcmp(first.serial, second.serial, sizeof(first.serial)));
    assert(!memcmp(first.pro_id, second.pro_id, sizeof(first.pro_id)));
    info[78] = 0xab;
    assert(ms_identity_decode(info, &second));
    assert(!memcmp(first.serial, second.serial, sizeof(first.serial)));
    assert(memcmp(first.pro_id, second.pro_id, sizeof(first.pro_id)));
    info[0] = 0;
    assert(!ms_identity_decode(info, &second) && !second.available);
    puts("PASS: bounded System Info, identity fields, placeholders, geometry and distinct IDs");
    return 0;
}
