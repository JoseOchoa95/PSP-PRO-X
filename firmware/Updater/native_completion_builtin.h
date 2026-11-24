#ifndef PROX_NATIVE_COMPLETION_BUILTIN_H
#define PROX_NATIVE_COMPLETION_BUILTIN_H

/* Returns the number of UTF-16 code units, including the terminator. */
int prox_completion_builtin_text(unsigned int language, unsigned short *out, unsigned int capacity);

#endif
