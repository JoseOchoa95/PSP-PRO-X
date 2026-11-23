#ifndef PROX_NATIVE_MOUNT_ACCESS_H
#define PROX_NATIVE_MOUNT_ACCESS_H
#include "prox_mount_access.h"
typedef struct {
    int (*view)(ProxMountView *);
    int (*open_owned)(void);
    int (*lock)(int);
    int (*unlock)(int);
    int64_t (*seek)(int,int64_t,int);
    int (*set_fd)(uint32_t,int);
    int (*set_flags)(uint32_t,uint32_t);
    int (*close_owned)(int);
} ProxMountOps;
typedef struct { int active,mapped,owned,fault;ProxMountView original; } ProxMountLease;
int prox_mount_access_cycle(const ProxMountOps *,ProxMountReport *);
int prox_mount_lease_begin(const ProxMountOps *,ProxMountLease *,ProxMountReport *);
int prox_mount_lease_end(const ProxMountOps *,int (*)(uint32_t),ProxMountLease *,ProxMountReport *);
int prox_native_mount_access_probe(ProxMountReport *);
int prox_native_mount_begin(ProxMountReport *);
int prox_native_mount_end(ProxMountReport *);
int prox_native_mount_active(void);
#endif
