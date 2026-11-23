#ifndef PROX_NATIVE_BACKGROUND_H
#define PROX_NATIVE_BACKGROUND_H
#include "update.h"
#include "prox_native_install.h"
int prox_native_background_prepare(const ProxJob *,const ProxManifest *,unsigned int,ProxNativeInstallJob *);
int prox_native_background_launch(const char *,const ProxNativeInstallJob *);
int prox_native_background_wait(void);
void prox_native_background_discard(const char *);
#endif
