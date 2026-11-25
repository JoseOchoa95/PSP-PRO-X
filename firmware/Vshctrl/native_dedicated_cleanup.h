#ifndef PROX_NATIVE_DEDICATED_CLEANUP_H
#define PROX_NATIVE_DEDICATED_CLEANUP_H
#include "prox_native_install.h"

/* Runs on the extended kernel stack. The service owns serialization.
 * -2 means flash access restoration failed and must remain recoverable. */
int prox_native_cleanup_flash(ProxNativeCleanupMessage *job);
#endif
