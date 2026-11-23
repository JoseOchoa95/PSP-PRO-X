#ifndef PROX_INSTALL_PREVIEW_SHIM_H
#define PROX_INSTALL_PREVIEW_SHIM_H
#define PSP_MODULE_INFO(a,b,c,d)
#define PSP_MAIN_THREAD_ATTR(a)
#define PSP_MAIN_THREAD_STACK_SIZE_KB(a)
#define PSP_HEAP_SIZE_KB(a)
unsigned int sceKernelDevkitVersion(void);
int scePowerLock(int);
int scePowerUnlock(int);
int scePowerTick(int);
int sceKernelDelayThread(unsigned int);
int kuKernelLoadModule(const char *,int,void *);
int sceKernelStartModule(int,unsigned int,void *,int *,void *);
#endif
