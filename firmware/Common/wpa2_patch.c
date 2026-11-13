/* WPA2 payload by Moment (@moment0253), ARK contributors and wpa2psp.
 * Port/validation by PRO-X. GNU GPL v3; see docs/upstream.md.
 * Pinned upstream code and SHA-256: docs/wpa2-upstream.json.
 */
#include "wpa2_patch.h"
#include <string.h>

static const unsigned char seed[] = {
    0x5c,0xf7,0x15,0x51,0xa2,0x56,0x46,0x30,
    0x25,0xe2,0x80,0x43,0x8a,0xf5,0x74,0xdc,
    0xd8,0xaf,0xe6,0x2e,0xca,0xab,0x52,0x8f,
    0x8a,0x11,0x93,0x07,0x74,0x54,0x12,0xf7
};
static const unsigned char rsn[] = {
    1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0
};
#define WORD(o,v) {o,4,4,1,0,v,NULL}
#define BYTE(o,v) {o,1,1,1,0,v,NULL}
const Wpa2Site prox_wpa2_sites[] = {
    WORD(0x14dc,0x24020004), WORD(0xef88,0x24050075),
    WORD(0xef98,0x24190002), WORD(0xf038,0x24080016),
    WORD(0xf054,0x24060030), WORD(0xf058,0x24050014),
    {0x11da8,20,1,0,0,0,rsn}, WORD(0xf06c,0x24060014),
    {0x11880,32,1,0,0,0,seed},
    WORD(0xf07c,0), WORD(0xf080,0), WORD(0xf0a0,0),
    WORD(0xf18c,0), WORD(0xf0b0,0), WORD(0xf0bc,0),
    WORD(0xf0c0,0), WORD(0xf0c8,0), WORD(0xf0cc,0), WORD(0xf168,0),
    BYTE(0xf104,0x79), BYTE(0xf134,0x75), BYTE(0xf1e8,2),
    BYTE(0xf680,2), BYTE(0xf518,2), BYTE(0xf344,2), BYTE(0xe0d0,2),
    {0xf220,2,2,1,0,0x308,NULL}, WORD(0xe9f8,0),
    {0xf2e0,4,4,1,1,0x3c050000,NULL},
    {0xf2e4,4,4,1,2,0x34a50000,NULL},
    WORD(0xf2e8,0x00a00008), WORD(0xf2ec,0x02009825)
};
typedef char site_count_check[(sizeof(prox_wpa2_sites) / sizeof(Wpa2Site)
                              == WPA2_SITE_COUNT) ? 1 : -1];

int prox_wpa2_range(const Wpa2Module *m, uint32_t off, size_t size,
                   unsigned int alignment, int code)
{
    unsigned int i;
    uintptr_t address;
    if (!m || !size || !alignment || !m->nsegment || m->nsegment > 4 ||
        m->text > UINTPTR_MAX - off)
        return 0;
    address = m->text + off;
    if (address % alignment || address > UINTPTR_MAX - size)
        return 0;
    if (code && (off > m->text_size || size > m->text_size - off))
        return 0;
    for (i = 0; i < m->nsegment; ++i) {
        uintptr_t start = m->segment[i].address;
        size_t length = m->segment[i].size;
        if (length <= UINTPTR_MAX - start && address >= start &&
            address - start <= length && size <= length - (address - start))
            return 1;
    }
    return 0;
}

void prox_wpa2_replacement(const Wpa2Module *m, unsigned int i,
                           unsigned char out[WPA2_MAX_BYTES])
{
    const Wpa2Site *s = &prox_wpa2_sites[i];
    uint32_t value = s->value;
    unsigned int j;
    if (s->bytes) {
        memcpy(out, s->bytes, s->size);
        return;
    }
    if (s->address_part) {
        uint32_t end = (uint32_t)(m->text + 0xf464);
        value |= s->address_part == 1 ? end >> 16 : end & 0xffff;
    }
    for (j = 0; j < s->size; ++j)
        out[j] = (unsigned char)(value >> (j * 8));
}

enum Wpa2Result prox_wpa2_apply(const Wpa2Module *m, int enabled,
                               const Wpa2Variant *v)
{
    size_t i;
    int original = 1, patched = 1;
    unsigned char replacement[WPA2_MAX_BYTES];
    unsigned char original_bytes[4];
    if (!enabled)
        return WPA2_DISABLED;
    if (!m || !v || !m->name || strcmp(m->name,"sceNetApctl_Library") ||
        m->firmware != WPA2_FIRMWARE || m->text_size != v->text_size ||
        m->data_size != v->data_size || m->bss_size != v->bss_size ||
        !m->nsegment || m->nsegment > 4 || m->nsegment != v->nsegment ||
        !v->guards || !v->guard_count || v->guard_count > 64)
        return WPA2_REJECTED;
    for (i = 0; i < m->nsegment; ++i) {
        if (m->text > UINTPTR_MAX - v->segment_offset[i] ||
            m->segment[i].address != m->text + v->segment_offset[i] ||
            m->segment[i].size != v->segment_size[i])
            return WPA2_REJECTED;
    }
    /* Check jump target separately, and ALL ranges before reading any site. */
    if (!prox_wpa2_range(m,0xf464,4,4,1))
        return WPA2_REJECTED;
    for (i = 0; i < WPA2_SITE_COUNT; ++i) {
        const Wpa2Site *s = &prox_wpa2_sites[i];
        if (!prox_wpa2_range(m,s->offset,s->size,s->alignment,s->code))
            return WPA2_REJECTED;
        if (v->original_jal_target[i]) {
            uint32_t target, pc;
            if (s->size != 4 || !s->code ||
                !prox_wpa2_range(m,v->original_jal_target[i],4,4,1))
                return WPA2_REJECTED;
            target = (uint32_t)(m->text + v->original_jal_target[i]);
            pc = (uint32_t)(m->text + s->offset + 4);
            if ((pc & 0xf0000000u) != (target & 0xf0000000u))
                return WPA2_REJECTED;
        }
    }
    for (i = 0; i < v->guard_count; ++i) {
        const Wpa2Guard *g = &v->guards[i];
        if (!g->size || g->size > WPA2_MAX_BYTES ||
            !prox_wpa2_range(m,g->offset,g->size,1,0) ||
            memcmp((const void *)(m->text + g->offset),g->bytes,g->size))
            return WPA2_REJECTED;
    }
    for (i = 0; i < WPA2_SITE_COUNT; ++i) {
        const Wpa2Site *s = &prox_wpa2_sites[i];
        const void *p = (const void *)(m->text + s->offset);
        const unsigned char *expected = v->original[i];
        if (v->original_jal_target[i]) {
            uint32_t target = (uint32_t)(m->text + v->original_jal_target[i]);
            uint32_t call = 0x0c000000u | ((target >> 2) & 0x03ffffffu);
            unsigned int byte;
            for (byte = 0; byte < 4; ++byte)
                original_bytes[byte] = (unsigned char)(call >> (8 * byte));
            expected = original_bytes;
        }
        prox_wpa2_replacement(m,i,replacement);
        if (memcmp(p,expected,s->size)) original = 0;
        if (memcmp(p,replacement,s->size)) patched = 0;
    }
    if (patched) return WPA2_ALREADY_APPLIED;
    if (!original) return WPA2_REJECTED;
    /* No validation can fail after this point. Caller synchronizes caches. */
    for (i = 0; i < WPA2_SITE_COUNT; ++i) {
        const Wpa2Site *s = &prox_wpa2_sites[i];
        prox_wpa2_replacement(m,i,replacement);
        memcpy((void *)(m->text + s->offset),replacement,s->size);
    }
    return WPA2_APPLIED;
}
