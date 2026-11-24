#ifndef PROX_NATIVE_TARGET_SHIM_H
#define PROX_NATIVE_TARGET_SHIM_H
#include "install_preview_loader_shim.h"
#include <stddef.h>
#define PSP_LOADMODULE_APITYPE_MS2 0x141
typedef struct { unsigned int size;char nsegment;int segmentaddr[4],segmentsize[4];char name[28]; } SceKernelModuleInfo;
int sceIoRmdir(const char *);
int sceIoMkdir(const char *,int);
int sceKernelQueryModuleInfo(int,SceKernelModuleInfo *);
int sceKernelUnloadModule(int);
int kuKernelLoadModuleWithApitype2(int,const char *,int,void *);
int mbedtls_sha256(const unsigned char *,size_t,unsigned char *,int);
#ifdef PROX_NATIVE_DEDICATED_OTA
typedef int SceUID;
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 0x200
#define PSP_O_EXCL 0x800
int sceIoOpen(const char *,int,int);
int sceIoWrite(int,const void *,unsigned int);
int sceIoClose(int);
#endif
#define PROX_NATIVE_ENGINE_SIZE 4u
#define PROX_NATIVE_ENGINE_SHA256 "unused-test-engine"
static const unsigned char prox_native_engine[4]={0x7f,'E','L','F'};
#endif
