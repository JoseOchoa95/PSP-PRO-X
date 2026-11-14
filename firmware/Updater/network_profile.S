.set noreorder
#include "pspimport.s"
/* Independent SDK-format header keeps this early object contiguous; sharing
 * the SDK archive's later header would interleave stubs and fail fixup.
 * Its firmware library name is still sceUtility, not the private symbol name. */
.section .rodata.sceResident, "a"
.align 2
prox_profile_library_name:
.asciz "sceUtility"
.align 2
.section .lib.stub, "a", @progbits
.global __stub_module_ProxUtilityProfile
__stub_module_ProxUtilityProfile:
.word prox_profile_library_name
.word 0x40090000
.word 0x5
.word __executable_start
.word __executable_start
IMPORT_FUNC "ProxUtilityProfile",0x4FED24D8,proxUtilityGetLatestProfile
