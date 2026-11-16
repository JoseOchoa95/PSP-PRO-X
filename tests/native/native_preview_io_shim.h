#ifndef PROX_NATIVE_PREVIEW_IO_SHIM_H
#define PROX_NATIVE_PREVIEW_IO_SHIM_H
int sceIoOpen(const char *,int,int);
int sceIoIoctl(int,unsigned int,void *,int,void *,int);
int sceIoDevctl(const char *,unsigned int,void *,int,void *,int);
int sceIoClose(int);
int prox_preview_test_bytes(const void *,unsigned int);
int sceKernelGetThreadId(void);
int sceKernelDelayThread(unsigned int);
unsigned int pspSdkDisableInterrupts(void);
void pspSdkEnableInterrupts(unsigned int);
#endif
