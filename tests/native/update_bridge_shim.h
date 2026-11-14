#ifndef UPDATE_BRIDGE_SHIM_H
#define UPDATE_BRIDGE_SHIM_H
#include <stddef.h>
extern unsigned int psp_fw_version, psp_model;
int pspSdkSetK1(int value);
int sceUtilsBufferCopyWithRange(void *, unsigned int, void *, int, int);
/* Poisoned SDK queries expose a regression to the PSP's observed error. */
int sceKernelDevkitVersion(void);
int sceKernelGetModel(void);
#endif
