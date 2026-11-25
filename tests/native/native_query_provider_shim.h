#ifndef PROX_NQ_PROVIDER_SHIM_H
#define PROX_NQ_PROVIDER_SHIM_H
int prox_native_test_valid(const void *,unsigned int);
int pspSdkSetK1(int);
int sceKernelExtendKernelStack(int,void (*)(void *),void *);
unsigned int pspSdkDisableInterrupts(void);
void pspSdkEnableInterrupts(unsigned int);
int sceKernelGetThreadId(void);
unsigned int sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned int);
void *sceKernelFindModuleByName(const char *);
#if defined(PROX_NATIVE_MOUNT_ACCESS) && defined(PROX_NATIVE_DEDICATED_OTA)
typedef unsigned int SceSize;
typedef int SceUID;
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned int,void *);
int sceKernelWaitThreadEnd(int,unsigned int *);
int sceKernelDeleteThread(int);
int sceKernelExitDeleteThread(int);
#endif
#endif
