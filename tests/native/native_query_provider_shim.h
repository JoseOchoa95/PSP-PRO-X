#ifndef PROX_NQ_PROVIDER_SHIM_H
#define PROX_NQ_PROVIDER_SHIM_H
int prox_native_test_valid(const void *,unsigned int);
int pspSdkSetK1(int);
unsigned int pspSdkDisableInterrupts(void);
void pspSdkEnableInterrupts(unsigned int);
int sceKernelGetThreadId(void);
unsigned int sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned int);
void *sceKernelFindModuleByName(const char *);
#endif
