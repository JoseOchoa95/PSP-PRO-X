/* GPL-3.0-or-later */
#include "probe.h"

#include <string.h>

static uint64_t read_le(const uint8_t *data, size_t size)
{
    uint64_t value = 0;
    while (size)
        value = (value << 8) | data[--size];
    return value;
}

static unsigned int read_be16(const uint8_t *data)
{
    return ((unsigned int)data[0] << 8) | data[1];
}

/* Memory Stick PRO System Information as returned by msstor on 6.61. */
unsigned decode_attribute_directory(const uint8_t *page, struct MsAttribute *entries)
{
    const unsigned capacity = 32768;
    unsigned count = page[4];
    if (page[0] != 0xa5 || page[1] != 0xc3 || !count || count > MS_ATTRIBUTE_MAX)
        return 0;
    unsigned header_bytes = 16 + 12 * count;
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *p = page + 16 + 12 * i;
        uint32_t offset = (uint32_t)read_be16(p) << 16 | read_be16(p + 2);
        uint32_t size = (uint32_t)read_be16(p + 4) << 16 | read_be16(p + 6);
        if (offset < header_bytes || offset >= capacity || size > capacity - offset)
            return 0;
        entries[i] = (struct MsAttribute){offset, size, p[8]};
    }
    return count;
}

int decode_media_geometry(struct MediaGeometry *geometry, const uint8_t *data)
{
    struct MediaGeometry parsed;
    uint64_t block_bytes;
    if (data[0] != 2)
        return 0;
    parsed.block_size = read_be16(data + 2);
    parsed.user_blocks = read_be16(data + 6);
    parsed.unit_bytes = read_be16(data + 44);
    block_bytes = (uint64_t)parsed.block_size * parsed.unit_bytes;
    if (!block_bytes || block_bytes % SECTOR_BYTES || !parsed.user_blocks)
        return 0;
    parsed.bytes = block_bytes * parsed.user_blocks;
    *geometry = parsed;
    return 1;
}

uint64_t primary_mbr_end(const uint8_t *sector)
{
    uint64_t starts[4], ends[4], highest = 0;
    size_t count = 0, i, j;
    if (sector[510] != 0x55 || sector[511] != 0xaa)
        return 0;
    for (i = 0; i < 4; i++) {
        const uint8_t *entry = sector + 446 + i * 16;
        unsigned int type = entry[4];
        uint64_t start = read_le(entry + 8, 4);
        uint64_t length = read_le(entry + 12, 4);
        uint64_t end = start + length;
        if (!type && !start && !length && !entry[0])
            continue;
        /* Protective and extended MBRs need their own layout readers. */
        if (!type || type == 0xee || type == 0x05 || type == 0x0f || type == 0x85 ||
            (entry[0] != 0 && entry[0] != 0x80) || !start || !length ||
            end > (UINT64_C(1) << 32))
            return 0;
        for (j = 0; j < count; j++)
            if (start < ends[j] && starts[j] < end)
                return 0;
        starts[count] = start;
        ends[count++] = end;
        if (end > highest)
            highest = end;
    }
    return highest;
}

uint64_t default_lba(enum ProbeIndex index, uint64_t sectors)
{
    switch (index) {
    case SECTOR_ZERO: return 0;
    case LOW_CONTROL: return SAMPLE_WINDOW;
    case BELOW_DECIMAL: return DECIMAL_CUTOFF - 1;
    case ABOVE_DECIMAL: return DECIMAL_CUTOFF;
    case BELOW_BINARY: return BINARY_CUTOFF - 1;
    case ABOVE_BINARY: return BINARY_CUTOFF;
    case NEAR_END: return sectors > 1 ? sectors - 2 : NO_LBA;
    case LAST_SECTOR: return sectors ? sectors - 1 : NO_LBA;
    default: return NO_LBA;
    }
}

static int valid_lba(enum ProbeIndex index, uint64_t lba, uint64_t sectors)
{
    uint64_t target = default_lba(index, sectors);
    if (lba == NO_LBA)
        return index > LOW_CONTROL && (target == NO_LBA || target >= sectors);
    if (lba >= sectors || target == NO_LBA)
        return 0;
    switch (index) {
    case BELOW_DECIMAL:
    case BELOW_BINARY:
    case NEAR_END:
        return lba <= target && target - lba < SAMPLE_WINDOW;
    case ABOVE_DECIMAL:
    case ABOVE_BINARY:
        return lba >= target && lba - target < SAMPLE_WINDOW;
    default:
        return lba == target;
    }
}

int decode_reference(struct Reference *reference, const uint8_t *data, size_t size)
{
    struct Reference parsed;
    uint64_t sectors;
    size_t i;
    if (size != REFERENCE_BYTES || memcmp(data, "MSDREF01", 8) ||
        read_le(data + 16, 4) != SECTOR_BYTES || read_le(data + 20, 4) != 0)
        return 0;
    parsed.bytes = read_le(data + 8, 8);
    if (parsed.bytes > INT64_MAX || parsed.bytes % SECTOR_BYTES)
        return 0;
    sectors = parsed.bytes / SECTOR_BYTES;
    for (i = 0; i < PROBE_COUNT; i++) {
        const uint8_t *record = data + REFERENCE_HEADER_BYTES + i * REFERENCE_RECORD_BYTES;
        parsed.sample[i].lba = read_le(record, 8);
        if (!valid_lba(i, parsed.sample[i].lba, sectors))
            return 0;
        memcpy(parsed.sample[i].digest, record + 8, DIGEST_BYTES);
    }
    *reference = parsed;
    return 1;
}

int sector_read_ok(const struct SectorRead *read)
{
    return read->bytes == SECTOR_BYTES && read->hash_result == 0;
}

int matches_reference(const struct SectorRead *read, const struct Sample *sample)
{
    return sector_read_ok(read) && !memcmp(read->digest, sample->digest, DIGEST_BYTES);
}

const char *sample_result(int identity_ok, const struct SectorRead *read,
                          const struct Sample *sample, const struct SectorRead *aliases,
                          size_t alias_count)
{
    size_t i;
    if (!sector_read_ok(read))
        return "Read failed";
    if (!sample)
        return "Read only; no PC comparison";
    if (!identity_ok)
        return "PC comparison withheld; controls differ";
    if (!matches_reference(read, sample)) {
        for (i = 0; i < alias_count; i++)
            if (sector_read_ok(&aliases[i]) &&
                !memcmp(read->digest, aliases[i].digest, DIGEST_BYTES))
                return "PC mismatch; matches lower LBA";
        return "PC mismatch";
    }
    if (read->uniform)
        return "PC match; uniform data, inconclusive";
    for (i = 0; i < alias_count; i++)
        if (!sector_read_ok(&aliases[i]) ||
            !memcmp(read->digest, aliases[i].digest, DIGEST_BYTES))
            return "PC match; lower LBA check inconclusive";
    return "PC match; distinct sample";
}
