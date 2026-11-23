#ifndef PROX_NATIVE_INSTALL_STATE_H
#define PROX_NATIVE_INSTALL_STATE_H
#include "prox_native_install.h"
typedef struct {
    unsigned int lease,generation,phase,last_seen;
    int worker,consumer,module,engine;ProxNativeInstallJob job;
} ProxNativeInstallState;
int prox_ni_job(const ProxNativeInstallJob *);
int prox_ni_arm(ProxNativeInstallState *,int,int,unsigned int,unsigned int,const char *,const ProxNativeInstallJob *,unsigned int);
int prox_ni_exchange(ProxNativeInstallState *,int,int,int,ProxNativeInstallMessage *,unsigned int);
int prox_ni_locked(const ProxNativeInstallState *);
#endif
