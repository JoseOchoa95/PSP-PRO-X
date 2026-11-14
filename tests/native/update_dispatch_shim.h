#ifndef UPDATE_DISPATCH_SHIM_H
#define UPDATE_DISPATCH_SHIM_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef unsigned int u32;
typedef struct {
    void *stub_top;
    unsigned int stub_size, entry_addr, text_addr, text_size, nsegment;
    unsigned int segmentaddr[4], segmentsize[4];
} SceModule2;
typedef SceModule2 SceModule;
typedef struct {
    const char *name;
    unsigned short version, attribute;
    unsigned char entLen, varCount;
    unsigned short funcCount;
    unsigned int *fnids, *funcs;
} PspModuleImport;
typedef struct { long st_size; } SceIoStat;
struct SceKernelLoadExecVSHParam { size_t size, args; void *argp; const char *key; };
struct Config { int useownupdate; };
extern struct Config conf;
extern unsigned int psp_fw_version;
#define FW_661 0x06060110
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
#define MAKE_SYSCALL(n) (((n)<<6)|12)
#define _sw(value, address) (*(unsigned int *)(uintptr_t)(address) = (value))
int pspSdkSetK1(int value);
int sceIoOpen(const char *, int, int);
int sceIoWrite(int, const void *, int);
int sceIoClose(int);
int sceIoGetstat(const char *, SceIoStat *);
int sctrlKernelLoadExecVSHMs2(const char *, struct SceKernelLoadExecVSHParam *);
int sctrlKernelLoadExecVSHEf2(const char *, struct SceKernelLoadExecVSHParam *);
unsigned int sctrlKernelQuerySystemCall(void *);
SceModule *sceKernelFindModuleByName(const char *);
#endif
