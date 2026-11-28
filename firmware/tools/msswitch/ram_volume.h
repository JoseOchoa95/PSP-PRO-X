/* GPL-3.0-or-later */
#ifndef MSSWITCH_RAM_VOLUME_H
#define MSSWITCH_RAM_VOLUME_H

#include "fat32_dirs.h"
#include "sony_mkdir_ram.h"

int ms_ram_volume_cleanup(void);
int ms_ram_volume_stage(struct Fat32Ram *output, uint32_t start, uint32_t length, uint32_t serial);

#ifdef MSDIRS_TRIAL
enum MsRamTrialStatus {
    MS_RAM_TRIAL_PREPARE,
    MS_RAM_TRIAL_OPEN,
    MS_RAM_TRIAL_RAN,
};

struct MsRamTrial {
    struct SonyMkdirResult created;
    unsigned writes;
    int created_all, tree, bounds, try_denied, denied, clean, open_error;
};

enum MsRamTrialStatus ms_ram_volume_test(uint32_t start, uint32_t length, uint32_t serial,
                                         struct Fat32Ram *output, struct MsRamTrial *trial);
#endif

#endif
