#ifndef PROX_NATIVE_FLASH_H
#define PROX_NATIVE_FLASH_H
#include "prox_native_install.h"
#include "prox_resident_slot.h"
typedef struct {
    unsigned int prepared,changed,originals,identical,backups;
    ProxNativePart before[PROX_NI_FLASH_PARTS];
    ProxNativePart worker;
    char worker_path[PROX_RESIDENT_SLOT_BYTES];
    int worker_prepared,worker_created,worker_cleanup;
} ProxNativeFlash;
const char *prox_native_flash_path(unsigned int);
/* Caller owns power/input/write gate and the writable flash mount. No config,
 * plugins, IPL, official firmware files or flash1 are touched here. Preparation
 * verifies ALL original backups before commit; caller must flush those backups
 * through the checked native lease END and reacquire access before commit.
 * flashfat 6.61 cannot rename: replacement/rollback use checked copies. */
int prox_native_flash_prepare(const ProxNativeInstallJob *,ProxNativeFlash *);
/* The separately verified worker from dedicated EBOOT part 11 is prepared
 * and committed first. Existing slots belonging to other builds stay intact. */
int prox_native_flash_prepare_dedicated(const ProxNativeInstallJob *,const ProxNativePart *,ProxNativeFlash *);
int prox_native_flash_commit(const ProxNativeInstallJob *,ProxNativeFlash *);
int prox_native_flash_rollback(const ProxNativeInstallJob *,ProxNativeFlash *);
int prox_native_flash_finalize(const ProxNativeInstallJob *,ProxNativeFlash *);
/* Dedicated backups survive commit. The caller must restore the recorded
 * transaction and supply the observed active build after a successful boot. */
int prox_native_flash_finalize_dedicated(const ProxNativeInstallJob *,ProxNativeFlash *,const char *active_build);
#endif
