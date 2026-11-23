#ifndef PROX_NATIVE_COMPLETION_TEXT_H
#define PROX_NATIVE_COMPLETION_TEXT_H
#include <stdio.h>
/* Reads one existing localized first line; never supplies fallback translations. */
int prox_completion_read_text(FILE *file,unsigned int language,unsigned short *out,unsigned int capacity);
#endif
