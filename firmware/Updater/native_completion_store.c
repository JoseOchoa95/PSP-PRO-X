#include "native_completion_store.h"
#include "native_completion_text.h"
#include "prox_completion_preview.h"
#include <mbedtls/sha256.h>

static const unsigned char header[16] = {'P','R','O','X','C','M','P','1',0x10,0x01,0x06,0x06,12,0,0,0};
#define INDEX_BYTES (sizeof(header) + 2u * PROX_COMPLETION_LANGUAGES)

static void put16(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static unsigned int get16(const unsigned char *p)
{
    return p[0] | ((unsigned int)p[1] << 8);
}

int prox_completion_store_valid(const unsigned char bytes[PROX_COMPLETION_STORE_BYTES])
{
    static const char digits[] = "0123456789abcdef";
    static const char expected[] = PROX_COMPLETION_STORE_SHA256;
    unsigned char digest[32];
    if (!bytes || memcmp(bytes, header, sizeof(header)) || mbedtls_sha256(bytes, PROX_COMPLETION_STORE_BYTES, digest, 0))
        return 0;
    for (unsigned int i = 0; i < sizeof(digest); ++i) {
        if (digits[digest[i] >> 4] != expected[2 * i] || digits[digest[i] & 15u] != expected[2 * i + 1])
            return 0;
    }
    return 1;
}

int prox_completion_pack(FILE *resource, unsigned char out[PROX_COMPLETION_STORE_BYTES])
{
    unsigned char bytes[PROX_COMPLETION_STORE_BYTES];
    unsigned short text[PROX_COMPLETION_UNITS];
    unsigned int position = INDEX_BYTES;
    if (!resource || !out)
        return -1;
    memcpy(bytes, header, sizeof(header));
    for (unsigned int language = 0; language < PROX_COMPLETION_LANGUAGES; ++language) {
        int units = prox_completion_read_text(resource, language, text, PROX_COMPLETION_UNITS);
        if (units < 2 || (unsigned int)units * 2 > sizeof(bytes) - position)
            return -1;
        put16(bytes + sizeof(header) + 2 * language, position);
        for (int unit = 0; unit < units; ++unit) {
            put16(bytes + position, text[unit]);
            position += 2;
        }
    }
    if (position != sizeof(bytes) || !prox_completion_store_valid(bytes))
        return -1;
    memcpy(out, bytes, sizeof(bytes));
    return 0;
}

int prox_completion_read_store(FILE *store, unsigned int language, unsigned short *out, unsigned int capacity)
{
    unsigned char bytes[PROX_COMPLETION_STORE_BYTES];
    if (!store || !out || language >= PROX_COMPLETION_LANGUAGES ||
        fread(bytes, 1, sizeof(bytes), store) != sizeof(bytes) || fgetc(store) != EOF || ferror(store) ||
        !prox_completion_store_valid(bytes))
        return -1;
    const unsigned char *index = bytes + sizeof(header) + 2 * language;
    unsigned int start = get16(index);
    unsigned int end = language + 1 == PROX_COMPLETION_LANGUAGES ? sizeof(bytes) : get16(index + 2);
    unsigned int units = (end - start) / 2;
    if (start < INDEX_BYTES || end > sizeof(bytes) || start >= end || ((start | end) & 1u) ||
        units < 2 || units > capacity || units > PROX_COMPLETION_UNITS || get16(bytes + end - 2))
        return -1;
    for (unsigned int unit = 0; unit < units; ++unit)
        out[unit] = (unsigned short)get16(bytes + start + 2 * unit);
    return (int)units;
}
