/* PRO-X phase 2. GPL-3.0-or-later. No Sony module bytes belong here. */
#ifndef PROX_WPA2_PATCH_H
#define PROX_WPA2_PATCH_H
#include <stddef.h>
#include <stdint.h>

#define WPA2_FIRMWARE 0x06060110u
#define WPA2_SITE_COUNT 32
#define WPA2_MAX_BYTES 32
enum Wpa2Result {
    WPA2_APPLIED, WPA2_ALREADY_APPLIED, WPA2_DISABLED,
    WPA2_REJECTED, WPA2_CONFIG_NOT_READY
};
typedef struct {
    uintptr_t address;
    size_t size;
} Wpa2Segment;
typedef struct {
    const char *name;
    uint32_t firmware;
    uintptr_t text;
    size_t text_size, data_size, bss_size;
    unsigned int nsegment;
    Wpa2Segment segment[4];
} Wpa2Module;
typedef struct {
    uint32_t offset;
    unsigned char size, alignment, code, address_part;
    uint32_t value;
    const unsigned char *bytes;
} Wpa2Site;
typedef struct {
    uint32_t offset;
    unsigned char size;
    unsigned char bytes[WPA2_MAX_BYTES];
} Wpa2Guard;
/* A future entry needs independently verified layout, ALL original sites and
 * unchanged guards. Relocated originals must be normalized from real evidence;
 * never populate this structure with guesses or captured replacement bytes. */
typedef struct {
    size_t text_size, data_size, bss_size;
    unsigned int nsegment;
    uint32_t segment_offset[4];
    size_t segment_size[4];
    unsigned char original[WPA2_SITE_COUNT][WPA2_MAX_BYTES];
    /* Nonzero entries encode an original JAL to a module-relative target.
     * Never compare a relocated call against another instance's address. */
    uint32_t original_jal_target[WPA2_SITE_COUNT];
    const Wpa2Guard *guards;
    size_t guard_count;
} Wpa2Variant;

extern const Wpa2Site prox_wpa2_sites[WPA2_SITE_COUNT];
int prox_wpa2_range(const Wpa2Module *mod, uint32_t offset, size_t size,
                   unsigned int alignment, int code);
void prox_wpa2_replacement(const Wpa2Module *mod, unsigned int site,
                           unsigned char out[WPA2_MAX_BYTES]);
enum Wpa2Result prox_wpa2_apply(const Wpa2Module *mod, int enabled,
                               const Wpa2Variant *variant);
#endif
