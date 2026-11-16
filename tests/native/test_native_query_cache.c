#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "native_cache_shim.h"
#include "native_query_cache.h"
static int mode,present=1,reads,opens,frees,loads,waits,position,allocated;
static unsigned char pbp[400];
static unsigned char *memory;
static int (*callback)(SceSize,void *);
static int load661(SceSize,void *,int,SceKernelLMOption *);
unsigned int sceKernelDevkitVersion(void) { return mode==11?0x06060010u:0x06060110u; }
static void put32(unsigned int offset,unsigned int value) { memcpy(pbp+offset,&value,4); }
static int allocate(int partition,const char *name,int type,unsigned int size,void *where)
{
    assert(partition==2 && strstr(name,"query cache") && type==1 && size==423 && !where);
    if(mode==4) return -1;
    memory=mmap((void *)0x09800000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(memory==(void *)0x09800000); allocated=1; return 19;
}
static void *head(int uid) { assert(uid==19 && allocated); return mode==7?(void *)0x01000000:memory; }
static int release(int uid) { assert(uid==19 && allocated); assert(!munmap(memory,4096)); allocated=0; ++frees; return 0; }
void *sctrlHENFindFunction(const char *m,const char *l,unsigned int nid)
{
    if(!strcmp(m,"sceModuleManager")) {
        assert(!strcmp(l,"ModuleMgrForKernel") && nid==0x4e62c48a);
        return mode==12?NULL:(void *)load661;
    }
    assert(!strcmp(m,"sceSystemMemoryManager") && !strcmp(l,"SysMemForKernel"));
    if(mode==5) return NULL;
    if(nid==0x237dbd4f) return (void *)allocate;
    if(nid==0x9d9a5ba1) return (void *)head;
    assert(nid==0xb6d61d02); return (void *)release;
}
void prox_native_query_event(const char *name,int result) { (void)name;(void)result; }
void *sceKernelFindModuleByName(const char *name) { assert(!strcmp(name,"ProxUpdater")); return NULL; }
int sceIoGetstat(const char *path,SceIoStat *stat)
{
    assert(present);
    if(mode==6) return -1;
    if(strncmp(path,mode==10?"ef0:":"ms0:",4)) return -1;
    stat->st_size=sizeof(pbp); return 0;
}
int sceIoOpen(const char *path,int flags,int access)
{ assert(present && strstr(path,"/PROXUPDATE/EBOOT.PBP") && flags==PSP_O_RDONLY && !access); ++opens; return 10; }
int sceIoRead(int fd,void *data,unsigned int size)
{
    assert(present && fd==10 && size<=sizeof(pbp)-(unsigned int)position); ++reads;
    if(mode==2 && position==40) return -1;
    memcpy(data,pbp+position,size); position+=(int)size; return (int)size;
}
int sceIoLseek(int fd,int offset,int origin) { assert(present && fd==10 && !origin); position=offset; return offset; }
int sceIoClose(int fd) { assert(present && fd==10); return 0; }
int sceKernelCreateThread(const char *name,int (*fn)(SceSize,void *),int priority,int stack,int attr,void *option)
{ assert(strstr(name,"query preparation") && priority==0x20 && stack==24576 && !attr && !option); callback=fn; return 8; }
int sceKernelStartThread(int uid,unsigned int size,void *arg) { assert(uid==8 && !size && !arg); callback(0,NULL); return 0; }
int sceKernelDeleteThread(int uid) { (void)uid; assert(0); return -1; }
int sceKernelExitDeleteThread(int status) { assert(!status); return 0; }
int sceKernelDelayThread(unsigned int time) { assert(time==100000 && ++waits<=50); return 0; }
void sceKernelDcacheWritebackInvalidateAll(void) {}
static int load661(SceSize size,void *buffer,int flags,SceKernelLMOption *option)
{
    assert(!present && allocated && !((uintptr_t)buffer&63u) && size==360 && !flags);
    assert(option->size==sizeof(*option) && option->access==1 && option->position==1);
    assert(!memcmp(buffer,pbp+40,360)); ++loads; return mode==8?-1:12;
}
/* Independent 6.61 binding of the OLD SDK import: raw a0/a1 pass through the
 * PRO NID map without an argument-order adapter. Old production source must
 * fail against this fixture, rather than share its incorrect prototype. */
int sceKernelLoadModuleBuffer(void *first,SceSize second,int flags,SceKernelLMOption *option)
{ return load661((SceSize)(uintptr_t)first,(void *)(uintptr_t)second,flags,option); }
static void scenario(void)
{
    unsigned int i; int rc,old_reads,old_opens; const char *path=NULL;
    memcpy(pbp,"\0PBP",4); put32(4,0x10000);
    for(i=0;i<7;++i) put32(8+4*i,40);
    put32(36,sizeof(pbp)); memcpy(pbp+40,"\177ELF",4); pbp[44]=pbp[45]=1; pbp[58]=8;
    memcpy(pbp+120,"PROX-NATIVE-QUERY-V3",20);
    memcpy(pbp+160,mode==1?"PROX-NATIVE-CARDLESS-V0":"PROX-NATIVE-CARDLESS-V1",23);
    if(mode==3) pbp[58]=0;
    if(mode==9) put32(32,0x300000);
    prox_native_query_prepare(); old_reads=reads; old_opens=opens;
    present=0; rc=prox_native_query_cached_load(&path);
    assert(reads==old_reads && opens==old_opens && !allocated);
    if(mode==0 || mode==10) assert(rc==12 && loads==1 && frees==1 && !strncmp(path,mode==10?"ef0:":"ms0:",4));
    else { assert(rc<0 && !path); assert(loads==(mode==8?1:0)); }
    assert(prox_native_query_cached_load(&path)<0); /* No stale executable reuse. */
}
int main(void)
{
    for(mode=0;mode<13;++mode) {
        int status; pid_t pid=fork(); assert(pid>=0);
        if(!pid) { scenario(); _exit(0); }
        assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    }
    puts("PASS: actual RAM bootstrap uses size-first 6.61 export after removal; bounds/alignment, wrong firmware/missing export/old marker/failure cleanup; legacy reversed binding rejected; no network/flash writes");
}
