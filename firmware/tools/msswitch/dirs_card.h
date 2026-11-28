/* GPL-3.0-or-later */
#ifndef MSSWITCH_DIRS_CARD_H
#define MSSWITCH_DIRS_CARD_H

#include <stdint.h>

struct Fat32Ram;
int dirs_card_load_reference(void);
int dirs_card_preview(void);
void dirs_card_write(struct Fat32Ram *image);
#ifdef MSMULTI_CARD_TRIAL
int dirs_card_stage(struct Fat32Ram *output, uint32_t start, uint32_t length, uint32_t serial);
void dirs_card_test_mount(void);
int dirs_card_preview_repeat(void);
#endif
int dirs_card_cleanup(void);
#if defined(MSAUTO_FORMAT_TRIAL) && !defined(MSWRITE_PATH_TRIAL)
int dirs_card_preview_current(void);
int dirs_card_start_monitor(void);
int dirs_card_stop_monitor(void);
int dirs_card_needs_qualification(void);
int dirs_card_qualify(void);
#endif

#endif
