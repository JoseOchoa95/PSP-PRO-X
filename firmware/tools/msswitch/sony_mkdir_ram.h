/* GPL-3.0-or-later */
#ifndef MSSWITCH_SONY_MKDIR_RAM_H
#define MSSWITCH_SONY_MKDIR_RAM_H

#include <stdint.h>

struct SonyMkdirResult {
    int result, unmount, mutex_close;
    unsigned created;
    int boundary_rejected;
};

int sony_mkdir_ram_check(void);
#ifdef MSMULTI_CARD_TRIAL
enum { SONY_FAT_MOUNTED = 0x08, SONY_FAT_WHOLE_DEVICE = 0x10 };
int sony_fat_stock_state(uint32_t *state, uint32_t *mounts);
#ifndef MSWRITE_PATH_TRIAL
enum SonyFatReprobe {
    SONY_FAT_REPROBED, SONY_FAT_REPROBE_BUSY, SONY_FAT_REPROBE_REMOVAL, SONY_FAT_REPROBE_INSERTION,
};
struct SonyFatDetection {
    int media_result, event_result, event_name_matches, waiters;
    uint32_t present, event, pending, activity, state, mounts, handle;
};
int sony_fat_stock_detection(struct SonyFatDetection *detection);
enum SonyFatReprobe sony_fat_stock_reprobe(void);
#ifdef MSAUTO_FORMAT_TRIAL
struct Fat32VolumeIdentity;
int sony_fat_stock_volume(struct Fat32VolumeIdentity *identity);
#endif
#endif
#endif
struct SonyMkdirResult sony_mkdir_ram_run(const char *device, int fd, unsigned sectors, int one_directory);

#endif
