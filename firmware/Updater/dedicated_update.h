/* Data-only dedicated PRO-X EBOOT reader. GPL-3.0-or-later. */
#ifndef PROX_DEDICATED_UPDATE_H
#define PROX_DEDICATED_UPDATE_H
#include <stddef.h>
#include "dedicated_format.h"
#include "prox_native_install.h"

#define PROX_DEDICATED_PARTS (PROX_NI_PARTS + 1u)
#define PROX_DEDICATED_WORKER PROX_NI_PARTS

typedef struct {
    unsigned int size;
    unsigned char sha256[32];
    unsigned int running[3], target[3];
    char build_id[19];
    unsigned int devkit, model, engine_revision, minimum_engine_revision;
} ProxDedicatedExpected;

typedef struct {
    unsigned int offset, size;
    unsigned char sha256[32];
} ProxDedicatedPart;

typedef struct {
    ProxDedicatedPart parts[PROX_DEDICATED_PARTS];
} ProxDedicatedUpdate;

/* Expected identity must come from an authenticated offer or build pin.
 * The caller owns the immutable input until it has consumed the parts.
 * Outputs are published only after every byte and part is verified. */
int prox_dedicated_update(const unsigned char *pbp, size_t size,
                          const ProxDedicatedExpected *expected, ProxDedicatedUpdate *out);
#endif
