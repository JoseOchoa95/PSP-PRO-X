#include "pspkernel.h"
#define PSP_SMEM_High 1
void *sctrlHENFindFunction(const char *,const char *,unsigned int);
int sceKernelDelayThread(unsigned int);
void sceKernelDcacheWritebackInvalidateAll(void);
int sceKernelLoadModuleBuffer(void *,SceSize,int,SceKernelLMOption *);
unsigned int sceKernelDevkitVersion(void);
