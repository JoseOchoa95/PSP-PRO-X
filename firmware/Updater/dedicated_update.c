/* Strict DATA.PSAR verification for dedicated updates. GPL-3.0-or-later. */
#include "dedicated_update.h"
#include "native_payload.h"
#include "policy.h"
#include <mbedtls/sha256.h>
#include <string.h>

#define PREFIX 64u
#define ENTRY 48u
#define HEADER (PREFIX + PROX_DEDICATED_PARTS * ENTRY)
#define PART_MAX (1024u * 1024u)
#define DEVKIT_661 0x06060110u
#define MODEL_MASK 2u

static unsigned int word(const unsigned char *p)
{
    return p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 | (unsigned int)p[3] << 24;
}

static int digest_matches(const unsigned char *data, size_t size, const unsigned char *expected)
{
    unsigned char hash[32];
    return !mbedtls_sha256(data, size, hash, 0) && !memcmp(hash, expected, sizeof(hash));
}

static int valid_build(const unsigned char *value)
{
    if (memcmp(value, "pro-x-", 6) || value[18] || value[19])
        return 0;
    for (unsigned int i = 6; i < 18; ++i) {
        unsigned char c = value[i];
        if ((c < '0' || c > '9') && (c < 'a' || c > 'f'))
            return 0;
    }
    return 1;
}

static unsigned int payload_start(const unsigned char *pbp, size_t size)
{
    unsigned int start = word(pbp + 36);
    if (memcmp(pbp, "\0PBP", 4) || word(pbp + 4) != 0x10000 || word(pbp + 8) != 40 ||
        start < 60 || start > 4096 || start > size - HEADER ||
        memcmp(pbp + 40, "\0PSF", 4) || word(pbp + 44) != 0x101)
        return 0;
    for (unsigned int i = 1; i < 7; ++i) {
        if (word(pbp + 8 + i * 4) != start)
            return 0;
    }
    return start;
}

int prox_dedicated_update(const unsigned char *pbp, size_t size,
                          const ProxDedicatedExpected *expected, ProxDedicatedUpdate *out)
{
    ProxDedicatedUpdate update;
    if (!pbp || !expected || !out || size < 60 + HEADER || size > PROX_DEDICATED_MAX_BYTES ||
        size != expected->size || expected->devkit != DEVKIT_661 || expected->model != 1 ||
        !digest_matches(pbp, size, expected->sha256))
        return -1;
    unsigned int start = payload_start(pbp, size);
    if (!start)
        return -1;
    const unsigned char *header = pbp + start;
    unsigned int version[] = {word(header + 28), word(header + 32), word(header + 36)};
    unsigned int engine = word(header + 24);
    if (memcmp(header, "PROXUPD\0", 8) || word(header + 8) != 1 || word(header + 12) != HEADER ||
        word(header + 16) != expected->devkit || word(header + 20) != MODEL_MASK ||
        !engine || engine > expected->engine_revision || engine != expected->minimum_engine_revision ||
        word(header + 60) != PROX_DEDICATED_PARTS ||
        !valid_build(header + 40) || memcmp(header + 40, expected->build_id, sizeof(expected->build_id)) ||
        prox_compare(version, expected->target) || prox_compare(version, expected->running) <= 0)
        return -1;
    for (unsigned int i = 0; i < 3; ++i) {
        if (version[i] > 2147483647u)
            return -1;
    }

    unsigned int cursor = HEADER;
    for (unsigned int i = 0; i < PROX_DEDICATED_PARTS; ++i) {
        const unsigned char *entry = header + PREFIX + i * ENTRY;
        unsigned int length = word(entry + 8), extent;
        char name[28];
        const char *required_name = i == PROX_DEDICATED_WORKER ? "ProxUpdater" : prox_native_payload_names[i];
        if (word(entry) != i || word(entry + 4) != cursor || length != word(entry + 12) ||
            length < 52 || length > PART_MAX || length > size - start - cursor ||
            !digest_matches(header + cursor, length, entry + 16) ||
            prox_native_image(header + cursor, length, name, &extent) ||
            extent != length || strcmp(name, required_name))
            return -1;
        update.parts[i].offset = start + cursor;
        update.parts[i].size = length;
        memcpy(update.parts[i].sha256, entry + 16, sizeof(update.parts[i].sha256));
        cursor += length;
    }
    if (cursor != size - start)
        return -1;
    *out = update;
    return 0;
}
