#ifndef PROX_OTA_INSTALLER_H
#define PROX_OTA_INSTALLER_H
#include "prox_native_install.h"
/* 0 manual install, 1 verified OTA request, -1 refuse before flash setup. */
int prox_ota_begin(int argc,char **argv,unsigned int firmware,unsigned int model);
int prox_ota_before_write(void);
int prox_ota_complete(void);
void prox_ota_result(int result);
void prox_ota_close(void);
int prox_ota_reboot_path(char path[256]);
int prox_ota_cleanup_modules(void);
#ifdef PROX_BACKGROUND_ENGINE
int prox_ota_native_begin(const ProxNativeInstallJob *,unsigned int,unsigned int);
int prox_ota_storage_commit(void);
int prox_ota_mark_committed(void);
#ifdef PROX_NATIVE_DEDICATED_OTA
int prox_ota_dedicated_trace(const ProxDedicatedJob *job);
int prox_ota_dedicated_lock(void);
#endif
#endif
#endif
