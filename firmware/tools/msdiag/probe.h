/* GPL-3.0-or-later */
#ifndef MSDIAG_PROBE_H
#define MSDIAG_PROBE_H

#include <stddef.h>
#include <stdint.h>

enum {
    SECTOR_BYTES = 512,
    DIGEST_BYTES = 20,
    MEDIA_ATTRIBUTE_BYTES = 96,
    SAMPLE_WINDOW = 2048,
    REFERENCE_HEADER_BYTES = 24,
    REFERENCE_RECORD_BYTES = 8 + DIGEST_BYTES,
};

#define DECIMAL_CUTOFF (UINT64_C(128000000000) / SECTOR_BYTES)
#define BINARY_CUTOFF (UINT64_C(128) * 1024 * 1024 * 1024 / SECTOR_BYTES)
#define NO_LBA UINT64_MAX

enum ProbeIndex {
    SECTOR_ZERO, LOW_CONTROL, BELOW_DECIMAL, ABOVE_DECIMAL,
    BELOW_BINARY, ABOVE_BINARY, NEAR_END, LAST_SECTOR, PROBE_COUNT
};

#define REFERENCE_BYTES (REFERENCE_HEADER_BYTES + PROBE_COUNT * REFERENCE_RECORD_BYTES)

struct Sample {
    uint64_t lba;
    uint8_t digest[DIGEST_BYTES];
};

struct Reference {
    uint64_t bytes;
    struct Sample sample[PROBE_COUNT];
};

struct SectorRead {
    int64_t seek;
    int bytes;
    int hash_result;
    int uniform;
    uint8_t digest[DIGEST_BYTES];
};

struct MediaGeometry {
    unsigned int block_size, user_blocks, unit_bytes;
    uint64_t bytes;
};

int decode_media_geometry(struct MediaGeometry *geometry, const uint8_t *data);
uint64_t primary_mbr_end(const uint8_t *sector);
int decode_reference(struct Reference *reference, const uint8_t *data, size_t size);
uint64_t default_lba(enum ProbeIndex index, uint64_t sectors);
int sector_read_ok(const struct SectorRead *read);
int matches_reference(const struct SectorRead *read, const struct Sample *sample);
const char *sample_result(int identity_ok, const struct SectorRead *read,
                          const struct Sample *sample, const struct SectorRead *aliases,
                          size_t alias_count);

#endif
