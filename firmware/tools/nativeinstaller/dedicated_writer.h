/* Dedicated job boundary for the local native installer. */
#ifndef PROX_DEDICATED_WRITER_H
#define PROX_DEDICATED_WRITER_H
#include "dedicated_job.h"
#include "native_flash.h"

/* Shared read-only entry used by the verified Settings handoff. */
int prox_dedicated_writer_check(const ProxDedicatedJob *job, unsigned int lease, unsigned int generation);
/* The caller owns the writable flash lease and backup flush sequence. */
int prox_dedicated_writer_prepare(const ProxDedicatedJob *job, unsigned int lease, unsigned int generation,
                                  ProxNativeFlash *transaction);
/* Call only after confirming the new active build. The flash lease must be
 * restored and flushed by the caller before it cleans the card. */
int prox_dedicated_writer_finalize(const ProxDedicatedJob *job, ProxNativeFlash *transaction,
                                   const char *active_build);
#endif
