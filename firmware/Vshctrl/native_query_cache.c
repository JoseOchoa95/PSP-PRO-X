/* Preserve the local user worker in RAM before removable storage is absent.
 * Loading/linking remains lazy, after the original network UI loads its libs.
 * GPL-3.0-or-later. */
#ifdef PROX_CACHE_TEST
#include "native_cache_shim.h"
#else
#include <pspkernel.h>
#include "systemctrl.h"
#endif
#include <stdint.h>
#include <string.h>
#include "prox_pbp_payload.h"
#include "native_query_cache.h"
#include "native_query_provider.h"
static volatile int preparing;
static unsigned char *cached;
static unsigned int cached_size;
static int cached_block=-1;
static const char *cached_path;
typedef int (*Alloc)(int,const char *,int,unsigned int,void *);
typedef void *(*Head)(int);
typedef int (*Free)(int);
/* The legacy SDK declaration is (buffer,size), but PRO's 6.61 resolver maps
 * its old NID BA889C07 directly to ModuleMgrForKernel_4E62C48A, whose native
 * ABI is (size,buffer). Resolve that export explicitly; do not use the old
 * import prototype or change inherited SDK/PRO compatibility headers. */
typedef int (*LoadBuffer661)(SceSize,void *,int,SceKernelLMOption *);
static Free release;
static int has_marker(const unsigned char *data,unsigned int size,const char *marker)
{
    unsigned int i,n=(unsigned int)strlen(marker)+1;
    if(size<n) return 0;
    for(i=0;i<=size-n;++i) if(!memcmp(data+i,marker,n)) return 1;
    return 0;
}
static int prepare(SceSize argc,void *argv)
{
    const char *paths[]={"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP","ef0:/PSP/GAME/PROXUPDATE/EBOOT.PBP"};
    Alloc allocate; Head head; unsigned char header[52],*buffer=NULL;
    unsigned int offset,length=0,total=0; int block=-1,fd=-1,i,wait;
    SceIoStat stat;
    (void)argc;(void)argv;
    if(sceKernelDevkitVersion()!=0x06060110u) goto done;
    allocate=(Alloc)sctrlHENFindFunction("sceSystemMemoryManager","SysMemForKernel",0x237dbd4f);
    head=(Head)sctrlHENFindFunction("sceSystemMemoryManager","SysMemForKernel",0x9d9a5ba1);
    release=(Free)sctrlHENFindFunction("sceSystemMemoryManager","SysMemForKernel",0xb6d61d02);
    if(!allocate || !head || !release) goto done;
    /* Initial mount can finish after VSH initialization. No unbounded waiter. */
    for(wait=0;wait<50;++wait) {
        for(i=0;i<2;++i) if(sceIoGetstat(paths[i],&stat)>=0) break;
        if(i<2) break;
        sceKernelDelayThread(100000);
    }
    if(i==2 || stat.st_size<92 || stat.st_size>16*1024*1024) goto done;
    fd=sceIoOpen(paths[i],PSP_O_RDONLY,0);
    if(fd<0 || sceIoRead(fd,header,40)!=40 ||
       prox_pbp_payload(header,(unsigned int)stat.st_size,&offset,&length) ||
       length<52 || length>2u*1024u*1024u) goto done;
    block=allocate(2,"PRO-X query cache",PSP_SMEM_High,length+63,NULL);
    if(block<0) goto done;
    buffer=head(block);
    if((uintptr_t)buffer<0x08800000u || (uintptr_t)buffer>0x0c000000u-length-63) goto done;
    buffer=(unsigned char *)(((uintptr_t)buffer+63u)&~(uintptr_t)63u);
    if(sceIoLseek(fd,offset,PSP_SEEK_SET)!=(int)offset) goto done;
    while(total<length) {
        unsigned int n=length-total; if(n>16384) n=16384;
        if(sceIoRead(fd,buffer+total,n)!=(int)n) goto done;
        total+=n;
    }
    if(memcmp(buffer,"\177ELF",4) || buffer[4]!=1 || buffer[5]!=1 || buffer[18]!=8 || buffer[19] ||
       !has_marker(buffer,length,"PROX-NATIVE-QUERY-V3") ||
       !has_marker(buffer,length,"PROX-NATIVE-CARDLESS-V1")) goto done;
    if(sceIoClose(fd)<0) { fd=-1; goto done; } fd=-1;
    cached_path=paths[i]; cached_size=length; cached_block=block; cached=buffer; block=-1;
    prox_native_query_event("native query RAM prepared",(int)length);
done:
    if(fd>=0) sceIoClose(fd);
    if(block>=0) release(block);
    if(!cached) prox_native_query_event("native query RAM unavailable",-1);
    preparing=0; sceKernelExitDeleteThread(0); return 0;
}
void prox_native_query_prepare(void)
{
    SceUID thread;
    if(preparing || cached || sceKernelFindModuleByName("ProxUpdater")) return;
    preparing=1;
    thread=sceKernelCreateThread("PRO-X query preparation",prepare,0x20,24576,0,NULL);
    if(thread<0) { preparing=0; return; }
    if(sceKernelStartThread(thread,0,NULL)<0) { preparing=0; sceKernelDeleteThread(thread); }
}
int prox_native_query_cached_load(const char **path)
{
    SceKernelLMOption option; LoadBuffer661 load; int module=-1,wait;
    /* Only this loader consumes the cache; publish it after all checks finish. */
    for(wait=0;preparing && wait<60;++wait) sceKernelDelayThread(100000);
    if(preparing || !cached || cached_block<0) return -1;
    memset(&option,0,sizeof(option)); option.size=sizeof(option); option.access=1; option.position=1;
    load=sceKernelDevkitVersion()==0x06060110u?(LoadBuffer661)sctrlHENFindFunction(
        "sceModuleManager","ModuleMgrForKernel",0x4e62c48a):NULL;
    if(!load) prox_native_query_event("native query RAM: 6.61 export unavailable",-1);
    else {
        sceKernelDcacheWritebackInvalidateAll();
        prox_native_query_event("native query RAM: size-first load",(int)cached_size);
        module=load(cached_size,cached,0,&option);
    }
    prox_native_query_event("native query RAM module load",module);
    /* ModuleMgr has copied its input before returning. Never retain an extra
     * executable-sized RAM buffer for the lifetime of the resident worker. */
    release(cached_block); cached_block=-1; cached=NULL; cached_size=0;
    if(module>=0) *path=cached_path;
    return module;
}
