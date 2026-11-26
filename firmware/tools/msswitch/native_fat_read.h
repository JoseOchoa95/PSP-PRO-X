/* GPL-3.0-or-later */
#ifndef MSSWITCH_NATIVE_FAT_READ_H
#define MSSWITCH_NATIVE_FAT_READ_H

#include <stdint.h>

#include "probe.h"

/* Private Sony 6.61 lower FAT context. Only the fields used by the three
   audited callbacks are populated. This object never enters Sony's registry. */
struct NativeFatContext {
    uint8_t unused_before_state[0x12c];
    uint32_t state, mounts, cached_state, unused_before_bounce;
    void *bounce;
    uint8_t unused_before_fd[0x90];
    int fd;
    uint8_t unused_before_device[0x1c];
    const char *device, *whole_device;
    void *backend;
};

struct NativeFatRead {
    uintptr_t text;
    struct NativeFatContext context;
    unsigned char sector[SECTOR_BYTES] __attribute__((aligned(64)));
};

struct Fat32Ram;

const char *native_fat_check(struct NativeFatRead *reader);
int native_fat_mount(struct NativeFatRead *reader, const char *device, int fd);
int native_fat_read(struct NativeFatRead *reader, uint32_t lba);
int native_fat_unmount(struct NativeFatRead *reader);
int native_fat_format_ram(const struct NativeFatRead *reader, struct Fat32Ram *volume);
#ifdef MSFORMAT_TRIAL
struct StockFormatTrial;
int native_fat_format_card(const struct NativeFatRead *reader, struct StockFormatTrial *trial);
#endif

#endif
