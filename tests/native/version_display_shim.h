#ifndef VERSION_DISPLAY_SHIM_H
#define VERSION_DISPLAY_SHIM_H
#include <stdint.h>
#define PSP_SMEM_High 1
uintptr_t sctrlHENFindFunction(char *, char *, unsigned int);
int pspSdkSetK1(int);
#endif
