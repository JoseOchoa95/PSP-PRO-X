#include "install_preview_shim.h"
#undef PSP_MAIN_THREAD_ATTR
#define PSP_MAIN_THREAD_ATTR(a) unsigned int sce_newlib_attribute=(a)
#define PSP_THREAD_ATTR_VSH 0xc0000000u
#define THREAD_ATTR_VFPU 0x00004000u
#define IOASSIGN_RDWR 0
#define IOASSIGN_RDONLY 1
int sceIoUnassign(const char *);
int sceIoAssign(const char *,const char *,const char *,int,void *,int);
int sceIoSync(const char *,int);
int sceKernelUnloadModule(int);
int kuKernelGetModel(void);
int kuKernelGetUserLevel(void);
