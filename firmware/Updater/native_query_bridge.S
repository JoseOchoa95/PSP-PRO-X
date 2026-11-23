.set noreorder
#include "pspimport.s"
IMPORT_START "ProxNativeQueryForUser",0x40090000
IMPORT_FUNC "ProxNativeQueryForUser",0x5e840c44,proxNativeQueryService
#ifdef PROX_NATIVE_BACKGROUND
IMPORT_FUNC "ProxNativeQueryForUser",0x474add30,proxNativeInstallService
#endif
