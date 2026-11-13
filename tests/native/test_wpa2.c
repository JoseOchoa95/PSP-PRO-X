/* Synthetic fixtures ONLY. These contain no Sony original instructions. */
#include "wpa2_patch.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char memory[0x13000], second[0x13000], before[0x13000];
static Wpa2Guard guard = {0x100,4,{0xaa,0xbb,0xcc,0xdd}};
static Wpa2Variant variant;
static Wpa2Module module;

static uint32_t word_at(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}

static void check_jump(const unsigned char *base)
{
    uint32_t target = (uint32_t)((uintptr_t)base+0xf464);
    assert(word_at(base+0xf2e0) == (0x3c050000u | target>>16));
    assert(word_at(base+0xf2e4) == (0x34a50000u | (target&0xffff)));
    assert(word_at(base+0xf2e8) == 0x00a00008u);
    assert(word_at(base+0xf2ec) == 0x02009825u);
}

static void fixture(unsigned char *base)
{
    unsigned int i;
    memset(base,0x5a,sizeof(memory));
    memset(&variant,0,sizeof(variant));
    memset(&module,0,sizeof(module));
    module.name = "sceNetApctl_Library";
    module.firmware = WPA2_FIRMWARE;
    module.text = (uintptr_t)base;
    module.text_size = variant.text_size = 0x11000;
    module.data_size = variant.data_size = 0x2000;
    module.nsegment = variant.nsegment = 2;
    module.segment[0].address = (uintptr_t)base;
    module.segment[0].size = variant.segment_size[0] = 0x11000;
    module.segment[1].address = (uintptr_t)base + 0x11000;
    variant.segment_offset[1] = 0x11000;
    module.segment[1].size = variant.segment_size[1] = 0x2000;
    variant.guards = &guard;
    variant.guard_count = 1;
    memcpy(base+guard.offset,guard.bytes,guard.size);
    for (i=0;i<WPA2_SITE_COUNT;++i)
        memcpy(variant.original[i],base+prox_wpa2_sites[i].offset,prox_wpa2_sites[i].size);
}

static void rejects_without_writes(void)
{
    memcpy(before,(void *)module.text,sizeof(before));
    assert(prox_wpa2_apply(&module,1,&variant) == WPA2_REJECTED);
    assert(!memcmp(before,(void *)module.text,sizeof(before)));
}

static void put_word(unsigned char *p,uint32_t value)
{
    unsigned int i;
    for (i=0;i<4;++i) p[i]=(unsigned char)(value>>(8*i));
}

static void add_relocated_original(unsigned char *base)
{
    /* Synthetic original call, not a Sony fixture. Different module-relative
     * target exercises the same JAL normalization used by production signatures. */
    uint32_t target=(uint32_t)((uintptr_t)base+0x1800);
    variant.original_jal_target[11]=0x1800;
    put_word(base+prox_wpa2_sites[11].offset,0x0c000000u|((target>>2)&0x03ffffffu));
}

int main(void)
{
    unsigned int i;
    unsigned char replacement[32];
    fixture(memory);
    assert(prox_wpa2_apply(&module,0,&variant) == WPA2_DISABLED);
    assert(prox_wpa2_apply(&module,1,NULL) == WPA2_REJECTED);
    assert(prox_wpa2_apply(&module,1,&variant) == WPA2_APPLIED);
    check_jump(memory);
    assert(word_at(memory+0x14dc) == 0x24020004u);
    assert(memory[0xf104] == 0x79 && memory[0xf134] == 0x75);
    memcpy(before,memory,sizeof(before));
    assert(prox_wpa2_apply(&module,1,&variant) == WPA2_ALREADY_APPLIED);
    assert(!memcmp(before,memory,sizeof(before)));
    for (i=0;i<WPA2_SITE_COUNT;++i) {
        prox_wpa2_replacement(&module,i,replacement);
        assert(!memcmp(memory+prox_wpa2_sites[i].offset,replacement,prox_wpa2_sites[i].size));
    }
    /* Reload at another base recalculates the MIPS address pair. */
    fixture(second);
    assert(prox_wpa2_apply(&module,1,&variant) == WPA2_APPLIED);
    check_jump(second);
    assert(word_at(second+0xf2e0) != word_at(memory+0xf2e0) ||
           word_at(second+0xf2e4) != word_at(memory+0xf2e4));
    fixture(memory);
    module.firmware--;
    rejects_without_writes();
    fixture(memory);
    add_relocated_original(memory);
    assert(prox_wpa2_apply(&module,1,&variant) == WPA2_APPLIED);
    assert(prox_wpa2_apply(&module,1,&variant) == WPA2_ALREADY_APPLIED);
    fixture(second);
    add_relocated_original(second);
    assert(prox_wpa2_apply(&module,1,&variant) == WPA2_APPLIED);
    fixture(memory);
    add_relocated_original(memory);
    memory[prox_wpa2_sites[11].offset] ^= 1;
    rejects_without_writes();
    fixture(memory);
    add_relocated_original(memory);
    variant.original_jal_target[11]=0x11000;
    rejects_without_writes();
    fixture(memory);
    variant.original_jal_target[19]=0x1800; /* A byte site cannot hold a JAL. */
    rejects_without_writes();
    fixture(memory);
    /* Reject an unencodable cross-256MB JAL BEFORE dereferencing this fake
     * layout. The production module is never read at fabricated addresses. */
    module.text=0x0fff1000u;
    module.segment[0].address=module.text;
    module.segment[1].address=module.text+0x11000;
    variant.original_jal_target[11]=0x1800;
    assert(prox_wpa2_apply(&module,1,&variant)==WPA2_REJECTED);
    fixture(memory);
    module.name = "other";
    rejects_without_writes();
    fixture(memory);
    memory[0x100] ^= 1;
    rejects_without_writes();
    fixture(memory);
    /* Last site signature mismatch must reject BEFORE the first write. */
    memory[0xf2ec] ^= 1;
    rejects_without_writes();
    fixture(memory);
    prox_wpa2_replacement(&module,0,replacement);
    memcpy(memory+prox_wpa2_sites[0].offset,replacement,4);
    rejects_without_writes();
    fixture(memory);
    module.segment[1].size = variant.segment_size[1] = 0x880;
    rejects_without_writes();
    fixture(memory);
    module.text_size = variant.text_size = 0xf464;
    rejects_without_writes();
    fixture(memory);
    module.nsegment = variant.nsegment = 5;
    rejects_without_writes();
    fixture(memory);
    assert(!prox_wpa2_range(&module,0xf221,2,2,1));
    assert(!prox_wpa2_range(&module,0x11000,1,1,1));
    assert(prox_wpa2_range(&module,0x11880,32,1,0));
    assert(!prox_wpa2_range(&module,0x12fff,2,1,0));
    assert(!prox_wpa2_range(&module,0,0,1,0));
    module.text = UINTPTR_MAX-1;
    assert(!prox_wpa2_range(&module,4,4,4,1));
    puts("PASS: synthetic WPA2 validation, zero-write rejection, repeat and reload");
    return 0;
}
