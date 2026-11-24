#ifndef PROX_NATIVE_SERVICE_SHIM_H
#define PROX_NATIVE_SERVICE_SHIM_H
#include <stddef.h>
typedef struct { int st_mode; } SceIoStat;
#define FIO_S_ISDIR(mode) ((mode) == 1)
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 0x200
#define PSP_O_EXCL 0x800
int sceIoGetstat(const char *,SceIoStat *);
int sceIoMkdir(const char *,int);
int sceIoOpen(const char *,int,int);
int sceIoWrite(int,const void *,unsigned int);
int sceIoClose(int);
int sceKernelDelayThread(unsigned int);
#endif
