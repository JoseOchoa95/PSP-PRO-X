#ifndef PROX_INSTALL_LOADER_SHIM_H
#define PROX_INSTALL_LOADER_SHIM_H
#include "install_preview_shim.h"
#define PSP_SYSTEMPARAM_ID_INT_LANGUAGE 8
#define PROX_PREVIEW_MODULE_SIZE 101u
#define PROX_PREVIEW_REBOOT_SIZE 102u
#define PROX_PREVIEW_MODULE_SHA256 "module-fixture-hash"
#define PROX_PREVIEW_REBOOT_SHA256 "reboot-fixture-hash"
int sceUtilityGetSystemParamInt(int,int *);
#endif
