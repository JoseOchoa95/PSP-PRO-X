#ifndef PROX_NATIVE_DEDICATED_STAGE_H
#define PROX_NATIVE_DEDICATED_STAGE_H
#include "native_download.h"
#include "dedicated_job.h"

#define PROX_DEDICATED_STAGE_STORAGE (-2)
#define PROX_DEDICATED_STAGE_POWER (-3)

/* Download only after consent. A successful stage remains owned by the
 * caller until its matching Settings handoff consumes or discards it. */
int prox_native_dedicated_stage(const char *directory, const char *ca, const unsigned int running[3],
                                unsigned int model, const ProxCatalogOffer *offer, int fixture,
                                unsigned int lease, unsigned int generation, ProxNativeProgress progress,
                                ProxDedicatedJob *out);
int prox_native_dedicated_discard(const ProxDedicatedJob *job);
#endif
