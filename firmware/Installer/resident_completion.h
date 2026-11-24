#ifndef PROX_RESIDENT_COMPLETION_H
#define PROX_RESIDENT_COMPLETION_H
#include "native_completion_store.h"

/* 1 means a verified private RCO supplied a new store, 0 means an existing
 * verified store or no optional input, and -1 means fail before flash writes. */
int prox_resident_completion_prepare(const char *installer_path, unsigned char store[PROX_COMPLETION_STORE_BYTES]);
int prox_resident_completion_install(const unsigned char store[PROX_COMPLETION_STORE_BYTES]);
#endif
