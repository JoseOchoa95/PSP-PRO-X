#ifndef PROX_RESOLVER_SHIM_H
#define PROX_RESOLVER_SHIM_H
#include <netinet/in.h>
typedef unsigned int SceSize;
typedef unsigned int SceUInt;
#define PSP_THREAD_ATTR_USER 0x80000000u
#define SCE_KERNEL_ERROR_WAIT_TIMEOUT ((int)0x800201a8)
int sceNetResolverCreate(int *,void *,SceSize);
int sceNetResolverDelete(int);
int sceNetResolverStartNtoA(int,const char *,struct in_addr *,unsigned int,int);
int sceNetResolverStop(int);
int sceNetApctlGetState(int *);
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,unsigned int,void *);
int sceKernelStartThread(int,unsigned int,void *);
int sceKernelWaitThreadEnd(int,SceUInt *);
int sceKernelDeleteThread(int);
int sceKernelExitGame(void);
int sceKernelExitDeleteThread(int);
unsigned int sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned int);
#endif
