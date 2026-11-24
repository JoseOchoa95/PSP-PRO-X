/* Bounded, read-only FAT12/16 volume capacity. GPL-3.0-or-later. */
#ifndef PROX_FLASH_CAPACITY_H
#define PROX_FLASH_CAPACITY_H

#include <stdint.h>

#define PROX_FLASH_SECTOR_BYTES 512u
#define PROX_FLASH_MAX_VOLUME_BYTES (64u * 1024u * 1024u)

typedef int (*ProxFlashRead)(void *context, uint32_t offset, void *buffer, unsigned int bytes);

typedef struct {
    uint32_t total_bytes;
    uint32_t free_bytes;
    unsigned int cluster_bytes;
    unsigned int fat_bits;
} ProxFlashCapacity;

enum {
    PROX_FLASH_CAPACITY_OK = 0,
    PROX_FLASH_CAPACITY_GEOMETRY = -1,
    PROX_FLASH_CAPACITY_READ = -2,
    PROX_FLASH_CAPACITY_FAT = -3
};

/* Reader returns zero only for a complete read. Only the boot sector and
 * allocation tables are read. The result is assigned only on success. */
int prox_flash_capacity(ProxFlashRead read, void *context, uint32_t volume_bytes, ProxFlashCapacity *out);
/* PSP 6.61 kernel context, with no writable flash lease held. */
int prox_flash0_capacity(ProxFlashCapacity *out);

#endif
