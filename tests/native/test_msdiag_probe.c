/* GPL-3.0-or-later */
#include "probe.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put_le(uint8_t *bytes, uint64_t value, size_t length)
{
    size_t i;
    for (i = 0; i < length; i++, value >>= 8)
        bytes[i] = value;
}

static void test_reference(void)
{
    const uint64_t bytes = UINT64_C(256000000000);
    uint8_t data[REFERENCE_BYTES + 1] = "MSDREF01";
    struct Reference reference;
    size_t i;
    put_le(data + 8, bytes, 8);
    put_le(data + 16, SECTOR_BYTES, 4);
    for (i = 0; i < PROBE_COUNT; i++) {
        uint8_t *record = data + REFERENCE_HEADER_BYTES + i * REFERENCE_RECORD_BYTES;
        put_le(record, default_lba(i, bytes / SECTOR_BYTES), 8);
        record[8] = (uint8_t)i;
    }
    assert(decode_reference(&reference, data, REFERENCE_BYTES));
    assert(reference.bytes == bytes);
    assert(reference.sample[ABOVE_DECIMAL].lba == 250000000);
    assert(reference.sample[ABOVE_BINARY].lba == 268435456);
    assert(reference.sample[LAST_SECTOR].lba == 499999999);
    assert(reference.sample[LAST_SECTOR].digest[0] == LAST_SECTOR);
    assert(!decode_reference(&reference, data, sizeof(data)));
    assert(!decode_reference(&reference, data, REFERENCE_BYTES - 1));
    data[0] = 'x';
    assert(!decode_reference(&reference, data, REFERENCE_BYTES));
    data[0] = 'M';
    put_le(data + 16, 4096, 4);
    assert(!decode_reference(&reference, data, REFERENCE_BYTES));
    put_le(data + 16, SECTOR_BYTES, 4);
    put_le(data + 8, UINT64_C(1) << 63, 8);
    assert(!decode_reference(&reference, data, REFERENCE_BYTES));
    put_le(data + 8, bytes, 8);
    put_le(data + REFERENCE_HEADER_BYTES + LAST_SECTOR * REFERENCE_RECORD_BYTES, NO_LBA, 8);
    assert(!decode_reference(&reference, data, REFERENCE_BYTES));
}

static void test_media_geometry(void)
{
    uint8_t attributes[MEDIA_ATTRIBUTE_BYTES] = {0};
    struct MediaGeometry geometry;
    attributes[0] = 2;
    attributes[2] = 0x20; /* 8,192 units per block */
    attributes[6] = 0xee;
    attributes[7] = 0xc0;
    attributes[44] = 2; /* 512 bytes per unit */
    assert(decode_media_geometry(&geometry, attributes));
    assert(geometry.block_size == 8192 && geometry.user_blocks == 61120 && geometry.unit_bytes == 512);
    assert(geometry.bytes == UINT64_C(256355860480));
    attributes[6] = 0x6e;
    assert(decode_media_geometry(&geometry, attributes));
    assert(geometry.bytes == UINT64_C(118916907008));
    attributes[0] = 1;
    assert(!decode_media_geometry(&geometry, attributes));
    attributes[0] = 2;
    attributes[44] = 0;
    assert(!decode_media_geometry(&geometry, attributes));
}

static void test_primary_mbr_end(void)
{
    uint8_t sector[SECTOR_BYTES] = {0};
    uint8_t *first = sector + 446;
    uint8_t *second = first + 16;
    first[4] = 7;
    put_le(first + 8, 2048, 4);
    put_le(first + 12, 500690944, 4);
    assert(primary_mbr_end(sector) == 0);
    sector[510] = 0x55;
    sector[511] = 0xaa;
    assert(primary_mbr_end(sector) == UINT64_C(500692992));
    second[4] = 0x0c;
    put_le(second + 8, 500692992, 4);
    put_le(second + 12, 2048, 4);
    assert(primary_mbr_end(sector) == UINT64_C(500695040));
    put_le(second + 8, 500692991, 4);
    assert(primary_mbr_end(sector) == 0);
    put_le(second + 8, UINT32_MAX, 4);
    assert(primary_mbr_end(sector) == 0);
    second[4] = 0xee;
    assert(primary_mbr_end(sector) == 0);
}

static void test_sample_results(void)
{
    struct Sample expected = {.digest = {1}};
    struct SectorRead read = {.bytes = SECTOR_BYTES, .digest = {1}};
    struct SectorRead alias = {.bytes = SECTOR_BYTES, .digest = {2}};
    assert(matches_reference(&read, &expected));
    assert(!strcmp(sample_result(1, &read, &expected, &alias, 1), "PC match; distinct sample"));
    assert(!strcmp(sample_result(0, &read, &expected, &alias, 1), "PC comparison withheld; controls differ"));
    assert(!strcmp(sample_result(1, &read, NULL, &alias, 1), "Read only; no PC comparison"));
    read.uniform = 1;
    assert(!strcmp(sample_result(1, &read, &expected, &alias, 1), "PC match; uniform data, inconclusive"));
    read.uniform = 0;
    alias.digest[0] = 1;
    assert(!strcmp(sample_result(1, &read, &expected, &alias, 1), "PC match; lower LBA check inconclusive"));
    alias.bytes = 0;
    assert(!strcmp(sample_result(1, &read, &expected, &alias, 1), "PC match; lower LBA check inconclusive"));
    alias.bytes = SECTOR_BYTES;
    expected.digest[0] = 2;
    assert(!strcmp(sample_result(1, &read, &expected, &alias, 1), "PC mismatch; matches lower LBA"));
    alias.digest[0] = 3;
    assert(!strcmp(sample_result(1, &read, &expected, &alias, 1), "PC mismatch"));
    read.bytes = 511;
    assert(!matches_reference(&read, &expected));
    assert(!strcmp(sample_result(1, &read, &expected, &alias, 1), "Read failed"));
    read.bytes = SECTOR_BYTES;
    read.hash_result = -1;
    assert(!sector_read_ok(&read));
}

int main(void)
{
    test_reference();
    test_media_geometry();
    test_primary_mbr_end();
    test_sample_results();
    puts("PASS: MSDIAG geometry, reference decoding and sample classification");
    return 0;
}
