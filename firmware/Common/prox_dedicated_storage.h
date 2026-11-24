/* Shared Memory Stick budget for verification and the native offer. */
#ifndef PROX_DEDICATED_STORAGE_H
#define PROX_DEDICATED_STORAGE_H
#define PROX_OFFER_TEXT_UNITS 120u

static inline unsigned long long prox_dedicated_card_bytes(unsigned int package_size)
{
    /* The EBOOT and its extracted parts coexist. Reserve up to 64 KiB
     * each for allocation rounding and directory growth. */
    return 2ull * package_size + 16u * 65536u;
}

static inline unsigned int prox_dedicated_card_mb(unsigned int package_size)
{
    return (unsigned int)((prox_dedicated_card_bytes(package_size) + 999999u) / 1000000u);
}
#endif
