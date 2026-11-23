#define _GNU_SOURCE
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PROX_MOUNT_PLATFORM_TEST 1
#include "../../firmware/Vshctrl/native_mount_platform.c"
static unsigned int base=0x880b7500u,table=0x880ce3b8u,fw=0x06060110u;static int model=1,held,opened,closed,missing;
unsigned int sceKernelDevkitVersion(void){return fw;}
int sceKernelGetModel(void){return model;}
void *sctrlHENFindDriver(const char *name){assert(!strcmp(name,"flashfat"));return (void *)(uintptr_t)(base+0xb74c);}
int sceIoOpen(const char *path,int flags,int mode){assert(!held && !strcmp(path,"lflash0:0,0") && flags==0x04000003 && !mode);++opened;return 29;}
int sceKernelLockMutex(int id,int count,SceUInt *timeout){assert(id==31 && count==1 && *timeout==1000000 && !held);held=1;return 0;}
int sceKernelUnlockMutex(int id,int count){assert(id==31 && count==1 && held);held=0;return 0;}
uintptr_t sctrlHENFindFunction(char *module,char *library,unsigned int nid)
{ assert(!strcmp(module,"sceThreadManager") && !strcmp(library,"ThreadManForKernel"));if(missing)return 0;if(nid==0xb011b11fu)return (uintptr_t)sceKernelLockMutex;assert(nid==0x6b30100fu);return (uintptr_t)sceKernelUnlockMutex; }
int64_t sceIoLseek(int fd,int64_t p,int whence){assert(held && (fd==17 || fd==29));if(whence==1){assert(!p);return 16384;}assert(p==16384 && whence==0);return p;}
int sceIoClose(int fd){assert(fd==29 && !held);++closed;return 0;}
void prox_native_query_event(const char *message,int result){(void)result;assert(message && !held);}
int sceIoSync(const char *path,int flags){assert(!held && !strcmp(path,"ms0:") && !flags);return 0;}
int prox_mount_platform_test_flush(uint32_t ptr,uint32_t driver)
{assert(held && ptr==table && driver==base && mount_lease.active);*(uint32_t *)(uintptr_t)table&=~4u;return 0;}
static void put(unsigned int a,unsigned int word){*(unsigned int *)(uintptr_t)a=word;}
static unsigned int jal(unsigned int a){return 0x0c000000u|((a>>2)&0x03ffffffu);}
static void reset(void)
{
    memset((void *)0x880b0000u,0,0x40000);fw=0x06060110u;model=1;held=opened=closed=missing=0;
    put(base+0xb74c+4,0x001e0010);put(base+0xb74c+16,base+0xb6f4);
    put(base+0xb6f4+18*4,base+0x2530);put(base+0xb6f4+19*4,base+0x2774);put(base+0xb6f4+20*4,base+0x286c);
    put(base+0x9004,0x3c030606);put(base+0x9008,0x03e00008);put(base+0x900c,0x34620110);
    put(base+0x5bb0,0x8e0c0008);put(base+0x5bb4,0x3c0d8001);put(base+0x5bb8,0x1d80ffee);put(base+0x5bbc,0x35a50018);
    put(base+0x5b58,0x3c02880c);put(base+0x5b5c,0x8c452fd4);put(base+0x5b8c,0x3c0a880c);put(base+0x5b90,0x8d482fd8);
    put(base+0xbad4,16);put(base+0xbad8,table);put(table,11);put(table+4,17);put(table+8,5);
    put(base+0x277c,0x3c14880c);put(base+0x27cc,0x8e912cc4);put(base+0xb7c4,31);
    put(base+0x27d8,jal(base+0xaf80));put(base+0x2820,jal(base+0xaf50));
    put(base+0x573c,0x3c0d0400);put(base+0x5744,0x018d8025);put(base+0x5750,jal(base+0xae60));
    put(base+0x5dc,jal(base+0xd3c));put(base+0xd3c,0x27bdfff0);put(base+0xd40,0xafb10004);put(base+0xd44,0x00808821);
    put(base+0xd54,0x8c860000);put(base+0xd58,0x30c40004);put(base+0xdb8,0x0440000b);put(base+0xddc,0x7c061084);put(base+0xde0,0xae260000);
}
int main(void)
{
    ProxMountReport r;unsigned int n;
    unsigned int bad[]={0xb74c+4,0xb74c+16,0xb6f4+18*4,0xb6f4+19*4,0xb6f4+20*4,0x9004,0x9008,0x900c,0x5bb0,0x5bb4,0x5bb8,0x5bbc,0x5b58,0x5b5c,0x5b8c,0x5b90,0xbad4,0xbad8,0x277c,0x27cc,0x27d8,0x2820,0x573c,0x5744,0x5750,0x5dc,0xd3c,0xd40,0xd44,0xd54,0xd58,0xdb8,0xddc,0xde0};
    void *p=mmap((void *)0x880b0000u,0x40000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(p==(void *)0x880b0000u);reset();assert(!prox_native_mount_access_probe(&r) && r.restored && r.refs_after==5 && r.flags_after==11 && opened==1 && closed==1 && !held);
    for(n=0;n<sizeof(bad)/sizeof(*bad);++n){reset();put(base+bad[n],0);assert(prox_native_mount_access_probe(&r)<0 && !opened && !closed);}
    reset();put(base+0xbad4,17);assert(prox_native_mount_access_probe(&r)<0 && !opened);
    reset();put(table+8,65536);assert(prox_native_mount_access_probe(&r)<0 && !opened);
    reset();put(table,9);assert(prox_native_mount_access_probe(&r)<0 && !opened);
    reset();model=0;assert(prox_native_mount_access_probe(&r)<0 && !opened);
    reset();fw=0x06060010;assert(prox_native_mount_access_probe(&r)<0 && !opened);
    reset();missing=1;assert(prox_native_mount_access_probe(&r)<0 && !opened && r.stage==-1);
    reset();assert(!prox_native_mount_begin(&r) && r.active && prox_native_mount_active() && !held && !closed);
    put(table,13);assert(!prox_native_mount_end(&r) && r.restored && !r.active && !prox_native_mount_active() && closed==1 && !held);
    assert(!munmap(p,0x40000));puts("PASS: actual PSP access adapter, exact driver/code/relocation/mutex/RW-open signatures, model/version/range/count/RO guards, no shell descriptor closed or firmware bytes written");
}
