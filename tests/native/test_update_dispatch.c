#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "update_dispatch_shim.h"
struct Config conf = {1};
unsigned int psp_fw_version = FW_661;
static SceModule *loaded;
static int launches, present;
int pspSdkSetK1(int value) { (void)value; return 0; }
int sceIoOpen(const char *p, int f, int m) { (void)p; (void)f; (void)m; return -1; }
int sceIoWrite(int f, const void *p, int n) { (void)f; (void)p; return n; }
int sceIoClose(int f) { (void)f; return 0; }
int sceIoGetstat(const char *p, SceIoStat *s) { (void)p; s->st_size=100; return present ? 0 : -1; }
int sctrlKernelLoadExecVSHMs2(const char *p, struct SceKernelLoadExecVSHParam *v)
{ assert(!strcmp(p,"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP")); assert(!strcmp(v->key,"game")); ++launches; return -5; }
int sctrlKernelLoadExecVSHEf2(const char *p, struct SceKernelLoadExecVSHParam *v)
{ (void)p; (void)v; ++launches; return -5; }
unsigned int sctrlKernelQuerySystemCall(void *f) { (void)f; return 7; }
SceModule *sceKernelFindModuleByName(const char *n) { (void)n; return loaded; }
void patch_update_plugin_module(SceModule *);
int proxInternetUpdate(void);

int main(void)
{
    unsigned char *memory = mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    SceModule module;
    PspModuleImport *table;
    unsigned int *nids, *stubs;
    assert(memory != MAP_FAILED && (uintptr_t)memory < 0xffffffff);
    memset(&module,0,sizeof(module)); memset(memory,0,4096);
    module.nsegment=1; module.segmentaddr[0]=(unsigned int)(uintptr_t)memory; module.segmentsize[0]=4096;
    module.stub_top=memory+512; module.stub_size=sizeof(PspModuleImport);
    module.text_addr=(unsigned int)(uintptr_t)memory; module.text_size=256;
    module.entry_addr=module.text_addr;
    table=(void *)(memory+512); table->entLen=sizeof(*table)/4;
    table->name=(char *)memory+768; strcpy((char *)table->name,"sceLibUpdateDL");
    table->funcCount=3; table->fnids=(void *)(memory+800); table->funcs=(void *)(memory+900);
    nids=table->fnids; stubs=table->funcs;
    nids[0]=0xFC1AB540; nids[1]=0xFD675E8D; nids[2]=0xFA9AA797;
    strcpy((char *)memory+128,"/UPDATE");
    stubs[4]=0x12345678; stubs[5]=0x23456789;
    loaded=&module;
    patch_update_plugin_module(&module);
    assert(stubs[0]==0x03e00008 && stubs[1]==MAKE_SYSCALL(7));
    assert(stubs[2]==0x03e00008 && stubs[3]==MAKE_SYSCALL(7));
    assert(stubs[4]==0x12345678 && stubs[5]==0x23456789);
    assert(!strcmp((char *)memory+128,"/UPDATE")); assert(*(unsigned int *)memory==0);
    /* Unknown/malformed imports refuse plugin startup, without modifying stubs. */
    stubs[0]=0xfeedface; table->entLen=0;
    patch_update_plugin_module(&module);
    assert(stubs[0]==0xfeedface); assert(*(unsigned int *)memory==0x03e00008);
    assert(*(unsigned int *)(memory+4)==0x3c028001);
    /* No guessed writes through out-of-bounds NID/stub arrays. */
    table->entLen=sizeof(*table)/4; table->funcs=(void *)(memory+4092);
    patch_update_plugin_module(&module); assert(stubs[0]==0xfeedface);
    table->funcs=(void *)(memory+901);
    patch_update_plugin_module(&module); assert(stubs[0]==0xfeedface);
    assert(proxInternetUpdate()<0 && launches==0); /* missing client */
    present=1; conf.useownupdate=0; assert(proxInternetUpdate()<0 && launches==0);
    conf.useownupdate=1; assert(proxInternetUpdate()==-5 && launches==1);
    munmap(memory,4096);
    puts("PASS: bounded XMB import dispatch, storage sentinel, unknown-layout blocking, missing/disabled client");
    return 0;
}
