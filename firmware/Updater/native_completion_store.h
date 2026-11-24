/* Verified compact form of the original 6.61 completion text. */
#ifndef PROX_NATIVE_COMPLETION_STORE_H
#define PROX_NATIVE_COMPLETION_STORE_H
#include <stdio.h>

#define PROX_COMPLETION_LANGUAGES 12u
#define PROX_COMPLETION_STORE_BYTES 522u
#define PROX_COMPLETION_STORE_PATH "flash0:/kd/_proxc661.bin"
#define PROX_COMPLETION_STORE_SHA256 "4b47ddf778c8df121a7fe3ade8acaada307ffd3e96d17834f568e29240557ba7"

/* The caller verifies provenance of the original RCO. Encoding also checks
 * the entire compact result before publishing it. */
int prox_completion_pack(FILE *resource, unsigned char out[PROX_COMPLETION_STORE_BYTES]);
int prox_completion_store_valid(const unsigned char bytes[PROX_COMPLETION_STORE_BYTES]);
int prox_completion_read_store(FILE *store, unsigned int language, unsigned short *out, unsigned int capacity);
#endif
