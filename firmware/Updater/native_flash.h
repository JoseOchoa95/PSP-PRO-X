#ifndef PROX_NATIVE_FLASH_H
#define PROX_NATIVE_FLASH_H
#include "prox_native_install.h"
typedef struct {
    unsigned int prepared,changed,originals,identical,backups;
    ProxNativePart before[10];
} ProxNativeFlash;
const char *prox_native_flash_path(unsigned int);
/* Caller owns power/input/write gate and the writable flash mount. No config,
 * plugins, IPL, official firmware files or flash1 are touched here. Preparation
 * verifies ALL original backups before commit; caller must flush those backups
 * through the checked native lease END and reacquire access before commit.
 * flashfat 6.61 cannot rename: replacement/rollback use checked copies. */
int prox_native_flash_prepare(const ProxNativeInstallJob *,ProxNativeFlash *);
int prox_native_flash_commit(const ProxNativeInstallJob *,ProxNativeFlash *);
int prox_native_flash_rollback(const ProxNativeInstallJob *,ProxNativeFlash *);
int prox_native_flash_finalize(const ProxNativeInstallJob *,ProxNativeFlash *);
#endif
