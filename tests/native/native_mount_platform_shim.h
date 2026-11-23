#include <stdint.h>
typedef unsigned int SceUInt;
unsigned int sceKernelDevkitVersion(void);
int sceKernelGetModel(void);
void *sctrlHENFindDriver(const char *);
uintptr_t sctrlHENFindFunction(char *,char *,unsigned int);
int sceIoOpen(const char *,int,int);
int sceKernelLockMutex(int,int,SceUInt *);
int sceKernelUnlockMutex(int,int);
int64_t sceIoLseek(int,int64_t,int);
int sceIoClose(int);
int sceIoSync(const char *,int);
int prox_mount_platform_test_flush(uint32_t,uint32_t);
