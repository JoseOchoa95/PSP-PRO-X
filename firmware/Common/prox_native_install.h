/* Private copied background-install job; no retained user pointers. */
#ifndef PROX_NATIVE_INSTALL_H
#define PROX_NATIVE_INSTALL_H
#include "prox_mount_access.h"
#define PROX_NI_PROTOCOL 1u
#define PROX_NI_PARTS 11u
#define PROX_NI_MODULE "ProxNativeInstaller"
#define PROX_NI_ARGUMENT "--prox-native-install-v1"
enum { PROX_NI_ARM=1,PROX_NI_BIND,PROX_NI_CLAIM,PROX_NI_STATUS,PROX_NI_WRITE,
       PROX_NI_COMMIT,PROX_NI_FAIL };
enum { PROX_NI_EMPTY=0,PROX_NI_PREPARED,PROX_NI_CLAIMED,PROX_NI_WRITING,
       PROX_NI_COMMITTED,PROX_NI_FAILED };
typedef struct { unsigned int size;char sha256[65]; } ProxNativePart;
typedef struct {
    char root[192],manifest_sha256[65],source_version[40],target_version[40],build_id[32];
    unsigned int model;ProxNativePart parts[PROX_NI_PARTS];
} ProxNativeInstallJob;
typedef struct {
    unsigned int protocol,lease,generation;int module,result;
    unsigned int phase,cancel,idle_us;ProxNativeInstallJob job;ProxMountReport access;
} ProxNativeInstallMessage;
int proxNativeInstallService(int,ProxNativeInstallMessage *,unsigned int);
#endif
