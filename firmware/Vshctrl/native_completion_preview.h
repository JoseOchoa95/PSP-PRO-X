#ifndef PROX_NATIVE_COMPLETION_PREVIEW_H
#define PROX_NATIVE_COMPLETION_PREVIEW_H
#include "prox_completion_preview.h"
void prox_native_completion_buffer(unsigned short *buffer);
void prox_native_completion_reset(void);
int prox_native_completion_arm(unsigned int generation,const unsigned short *,unsigned int count);
int prox_native_completion_ready(unsigned int generation);
int prox_native_completion_armed(unsigned int generation);
void prox_native_completion_guard(int ready);
int prox_native_completion_health(unsigned int generation,unsigned int now);
void prox_native_completion_buttons(unsigned int buttons);
/* Called with interrupts disabled only for bounded state operations. */
#endif
