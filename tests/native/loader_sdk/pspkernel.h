#ifndef PROX_LOADER_SDK_H
#define PROX_LOADER_SDK_H
typedef unsigned int SceSize;
typedef int SceUID;
typedef struct { long long st_size; unsigned int st_mode; } SceIoStat;
typedef struct { unsigned int size; int access,position; } SceKernelLMOption;
#define PSP_O_RDONLY 1
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 0x200
#define PSP_O_EXCL 0x800
#define PSP_SEEK_SET 0
#define FIO_S_ISREG(mode) ((mode)==1)
#define FIO_S_ISDIR(mode) ((mode)==2)
unsigned int pspSdkDisableInterrupts(void);
void pspSdkEnableInterrupts(unsigned int);
int sceIoGetstat(const char *,SceIoStat *);
int sceIoOpen(const char *,int,int);
int sceIoRead(int,void *,unsigned int);
int sceIoWrite(int,const void *,unsigned int);
int sceIoClose(int);
int sceIoLseek(int,int,int);
int sceIoRemove(const char *);
int sceIoMkdir(const char *,int);
int sceIoRmdir(const char *);
int sceIoSync(const char *, unsigned int);
int sceKernelDelayThread(unsigned int);
void *sceKernelFindModuleByName(const char *);
int sceKernelLoadModule(const char *,int,SceKernelLMOption *);
int sceKernelStartModule(int,unsigned int,void *,int *,void *);
int sceKernelUnloadModule(int);
int sceKernelExitDeleteThread(int);
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned int,void *);
int sceKernelDeleteThread(int);
int MScmIsMediumInserted(void);
#endif
