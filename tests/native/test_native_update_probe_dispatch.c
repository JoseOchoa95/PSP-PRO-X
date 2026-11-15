#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "update_dispatch_shim.h"
#include "native_update_probe.h"
struct Config conf={1}; unsigned int psp_fw_version=FW_661;
static SceModule *loaded; static int missing;
int pspSdkSetK1(int x) { (void)x; return 0; }
int sceIoOpen(const char *p,int f,int m) { (void)p; (void)f; (void)m; return -1; }
int sceIoWrite(int f,const void *p,int n) { (void)f; (void)p; return n; }
int sceIoClose(int f) { (void)f; return 0; }
int sceIoGetstat(const char *p,SceIoStat *s) { (void)p; (void)s; return -1; }
int prox_start_xmb_update(void) { assert(0); return -1; }
int sctrlKernelLoadExecVSHMs2(const char *p,struct SceKernelLoadExecVSHParam *v)
{ (void)p; (void)v; assert(0); return -1; }
int sctrlKernelLoadExecVSHEf2(const char *p,struct SceKernelLoadExecVSHParam *v)
{ (void)p; (void)v; assert(0); return -1; }
unsigned int sctrlKernelQuerySystemCall(void *f) { assert(f); return missing?(unsigned int)-1:7; }
SceModule *sceKernelFindModuleByName(const char *n) { (void)n; return loaded; }
int prox_probe_test_word(unsigned int *p) { (void)p; return 0; }
void patch_update_plugin_module(SceModule *);
int main(void)
{
    static const unsigned int expected[]={0x34243b86,0x36d8f34b,0x4f49c9c1,0x88ff3935,0xb5db018d,
        0xc1af1076,0xd6a09757,0xf6690a9a,0xfa9aa797,0xfd675e8d};
    unsigned char *memory=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    SceModule module; PspModuleImport *table; unsigned int *nids,*stubs,i;
    assert(memory!=MAP_FAILED && (uintptr_t)memory<0xffffffff);
    memset(&module,0,sizeof(module)); memset(memory,0,4096);
    module.nsegment=1; module.segmentaddr[0]=(unsigned int)(uintptr_t)memory; module.segmentsize[0]=4096;
    module.stub_top=memory+512; module.stub_size=sizeof(PspModuleImport);
    module.text_addr=(unsigned int)(uintptr_t)memory; module.text_size=256; module.entry_addr=module.text_addr;
    table=(void *)(memory+512); table->entLen=sizeof(*table)/4;
    table->name=(char *)memory+768; strcpy((char *)table->name,"sceLibUpdateDL");
    table->funcCount=10; table->fnids=(void *)(memory+800); table->funcs=(void *)(memory+1000);
    nids=table->fnids; stubs=table->funcs; memcpy(nids,expected,sizeof(expected)); loaded=&module;
    *(unsigned int *)memory=0x27bdfff0; *(unsigned int *)(memory+4)=0xafbf0000;
    patch_update_plugin_module(&module);
    assert(*(unsigned int *)memory==0x27bdfff0 && *(unsigned int *)(memory+4)==0xafbf0000);
    for(i=0;i<10;++i) assert(stubs[2*i]==0x03e00008 && stubs[2*i+1]==MAKE_SYSCALL(7));
    memset(stubs,0x5a,80); nids[9]=nids[0];
    patch_update_plugin_module(&module);
    for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
    assert(*(unsigned int *)memory==0x03e00008 && *(unsigned int *)(memory+4)==0x3c028001);
    memcpy(nids,expected,sizeof(expected)); missing=1; patch_update_plugin_module(&module);
    for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
    missing=0; conf.useownupdate=0; patch_update_plugin_module(&module);
    for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
    munmap(memory,4096);
    puts("PASS: all-ten native imports, preserved original entry, atomic rejection, disabled/missing syscall fail closed");
    return 0;
}
