/* Verified card-stage data for a dedicated update. GPL-3.0-or-later. */
#ifndef PROX_DEDICATED_JOB_H
#define PROX_DEDICATED_JOB_H
#include "dedicated_update.h"
#include "native_flash.h"

/* Root is owned by this transaction and contains a closed EBOOT.PBP. The
 * native directory must not exist. Publish the job only after read-back. */
int prox_dedicated_prepare(const char *root, const ProxDedicatedExpected *expected,
                           const char *manifest_hash, ProxDedicatedJob *out);
/* Check the copied job against the retained EBOOT and the entire staged
 * inventory using bounded reads. Safe for the local writer's small heap. */
int prox_dedicated_verify(const ProxDedicatedJob *job);
/* Persist the committed card inventory after the caller has verified the
 * flash flush. A valid record survives interruption and is scoped per card. */
int prox_dedicated_record_available(const ProxDedicatedJob *job);
/* 1 pending regular record, 0 absent, -1 storage/metadata error. */
int prox_dedicated_pending(const char *directory);
int prox_dedicated_record_commit(const ProxDedicatedJob *job, const ProxNativeFlash *transaction);
/* Read back the verified target and its exact flash-sidecar fingerprints. */
int prox_dedicated_read_committed(const char *directory, const char *active_build, const char *active_version,
                                  unsigned int model, ProxDedicatedJob *job, ProxNativeFlash *transaction);
/* Call once the active build is verified and flash finalization is flushed.
 * Retry uses the same committed record and preserves foreign files. */
int prox_dedicated_cleanup_committed(const char *directory, const char *active_build, const char *active_version,
                                     unsigned int model);
/* Call only for a successfully prepared job in its still-owned stage. */
int prox_dedicated_remove_parts(const ProxDedicatedJob *job);
#endif
