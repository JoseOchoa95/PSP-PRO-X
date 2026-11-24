#ifndef PROX_RESIDENT_LOADER_SHIM_H
#define PROX_RESIDENT_LOADER_SHIM_H

typedef unsigned int SceSize;
typedef int SceUID;
typedef struct { long long st_size; int st_mode; } SceIoStat;
typedef struct { unsigned int size; int access, position; } SceKernelLMOption;
typedef struct { unsigned int size, stacksize; } SceKernelSMOption;
#define FIO_S_ISREG(mode) ((mode) == 1)
#define PSP_SMEM_High 1
#define PSP_O_RDONLY 0x0001
#define PSP_O_WRONLY 0x0002
#define PSP_O_CREAT 0x0200
#define PSP_O_TRUNC 0x0400
#define PSP_O_EXCL 0x0800
unsigned int sceKernelDevkitVersion(void);
int sceKernelGetModel(void);
void sceKernelDcacheWritebackInvalidateAll(void);
void *sctrlHENFindFunction(const char *, const char *, unsigned int);
int sceIoGetstat(const char *, SceIoStat *);
int sceIoOpen(const char *, int, int);
int sceIoRead(int, void *, unsigned int);
int sceIoWrite(int, const void *, unsigned int);
int sceIoClose(int);
int sceIoRemove(const char *);
int kuKernelLoadModule(const char *, int, SceKernelLMOption *);
int sceKernelStartModule(int, SceSize, void *, int *, SceKernelSMOption *);
int sceKernelStopModule(int, SceSize, void *, int *, SceKernelSMOption *);
int sceKernelUnloadModule(int);
void pspDebugScreenPrintf(const char *, ...);

#endif
