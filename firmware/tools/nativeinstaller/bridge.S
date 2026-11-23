.set noreorder
#include "pspimport.s"
IMPORT_START "ProxNativeQueryForUser",0x40090000
IMPORT_FUNC "ProxNativeQueryForUser",0x474add30,proxNativeInstallService
IMPORT_FUNC "ProxNativeQueryForUser",0x6cbb1855,proxNativeCompletionControl
IMPORT_START "ProxUpdateForUser",0x40090000
IMPORT_FUNC "ProxUpdateForUser",0xba1d93ca,proxUpdateGetState
