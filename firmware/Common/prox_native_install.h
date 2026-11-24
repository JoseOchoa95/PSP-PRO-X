/* Private copied background-install job; no retained user pointers. */
#ifndef PROX_NATIVE_INSTALL_H
#define PROX_NATIVE_INSTALL_H
#include "prox_mount_access.h"
#define PROX_NI_PARTS 11u
#define PROX_NI_FLASH_PARTS 10u
#define PROX_NI_MODULE "ProxNativeInstaller"
#ifdef PROX_NATIVE_DEDICATED_OTA
#define PROX_NI_PROTOCOL 2u
#define PROX_NI_ARGUMENT "--prox-native-install-v2"
#else
#define PROX_NI_PROTOCOL 1u
#define PROX_NI_ARGUMENT "--prox-native-install-v1"
#endif
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
    ProxNativeInstallJob install;
    ProxNativePart worker,package;
} ProxDedicatedJob;
typedef struct {
    unsigned int protocol,lease,generation;int module,result;
    unsigned int phase,cancel,idle_us;ProxNativeInstallJob job;ProxMountReport access;
#ifdef PROX_NATIVE_DEDICATED_OTA
    ProxNativePart worker,package;
#endif
} ProxNativeInstallMessage;
#ifdef PROX_NATIVE_DEDICATED_OTA
/* Separate cleanup-only packet on the private install service. No user
 * pointers and no mechanism for supplying writable flash paths. */
#define PROX_NI_CLEANUP_COMMITTED 12
#define PROX_NI_CLEANUP_PROTOCOL 1u
typedef struct {
    unsigned int protocol,lease,generation,model;
    char build_id[32];
    unsigned int prepared,backups,worker_prepared;
    ProxNativePart installed[PROX_NI_FLASH_PARTS],worker,before[PROX_NI_FLASH_PARTS];
} ProxNativeCleanupMessage;
#endif
int proxNativeInstallService(int,ProxNativeInstallMessage *,unsigned int);
#endif
