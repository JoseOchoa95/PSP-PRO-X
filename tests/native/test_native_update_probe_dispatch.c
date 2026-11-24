#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "update_dispatch_shim.h"
#include "native_update_probe.h"
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
#include "native_offer_preview.h"
#endif
struct Config conf={1}; unsigned int psp_fw_version=FW_661;
static SceModule *loaded; static int missing;
int pspSdkSetK1(int x) { (void)x; return 0; }
int sceIoOpen(const char *p,int f,int m) { (void)p; (void)f; (void)m; return -1; }
int sceIoWrite(int f,const void *p,int n) { (void)f; (void)p; return n; }
int sceIoClose(int f) { (void)f; return 0; }
int sceIoGetstat(const char *p,SceIoStat *s) { (void)p; (void)s; return -1; }
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
int sceIoIoctl(int f,unsigned int c,void *i,int n,void *o,int z)
{ (void)f;(void)c;(void)i;(void)n;(void)o;(void)z; assert(0); return -1; }
int sceIoDevctl(const char *p,unsigned int c,void *i,int n,void *o,int z)
{ (void)p;(void)c;(void)i;(void)n;(void)o;(void)z; assert(0); return -1; }
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
#ifndef PROX_NATIVE_OTA
int prox_native_flash_capacity_preview(void) { assert(0); return -1; }
#endif
int prox_preview_test_bytes(const void *p,unsigned int n) { (void)p;(void)n; assert(0); return 0; }
int sceKernelGetThreadId(void) { assert(0); return 0; }
unsigned int pspSdkDisableInterrupts(void) { assert(0); return 0; }
void pspSdkEnableInterrupts(unsigned int x) { (void)x; assert(0); }
int sceKernelDelayThread(unsigned int us) { (void)us; assert(0); return 0; }
#endif
#endif
int prox_start_xmb_update(void) { assert(0); return -1; }
#ifdef PROX_NATIVE_OTA
static int redirect_missing;
#ifdef PROX_NATIVE_INSTALL_PREVIEW
static int completion_guard;
static int back_missing;
void prox_native_completion_guard(int ready) { completion_guard=ready; }
unsigned int prox_native_completion_noop_address(void) { return 0x08812400; }
unsigned int prox_native_completion_back_address(void) { return back_missing?0:0x08812500; }
#endif
unsigned int prox_native_error_prepare(void) { return redirect_missing?0:0x08812340; }
int prox_native_ota_begin(int owner,unsigned int total) { (void)owner;(void)total; assert(0); return -1; }
int prox_native_ota_wait(int owner,unsigned int received,unsigned int wanted)
{ (void)owner;(void)received;(void)wanted; assert(0); return -1; }
#endif
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
#ifdef PROX_NATIVE_INSTALL_PREVIEW
    {
        unsigned int *events=(void *)(memory+3120);
        strcpy((char *)memory+3300,"onCancelNoNetworkUpdate");
        strcpy((char *)memory+3340,"onPushNetworkDownloadComplete");
        strcpy((char *)memory+3380,"onInitNetworkDownload");
        events[0]=(unsigned int)(uintptr_t)(memory+3300);events[1]=module.text_addr+80;events[2]=0;
        events[3]=(unsigned int)(uintptr_t)(memory+3340);events[4]=module.text_addr+64;events[5]=0;
        events[6]=(unsigned int)(uintptr_t)(memory+3380);events[7]=module.text_addr+100;events[8]=0;
        *(unsigned int *)(memory+64)=0x27bdff20;*(unsigned int *)(memory+68)=0xafbf00dc;
    }
#endif
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
    {
        static const unsigned int io_nids[]={0x42ec03ac,0x54f5fb11,0x63632449,0x6a638d83,0x779103a0,
            0x810c4bc3,0x06a70004,0xace946e8,0xb29ddf9c,0xb8a740f4,0xeb092469,0xf27a9c51,
            0x109f50bc,0x1117c65f,0x27eb27b8};
        PspModuleImport *io=(void *)(memory+512+sizeof(PspModuleImport));
        module.stub_size=2*sizeof(PspModuleImport); io->entLen=sizeof(*io)/4;
        io->name=(char *)memory+1200; strcpy((char *)io->name,"IoFileMgrForUser");
        io->funcCount=15; io->fnids=(void *)(memory+1256); io->funcs=(void *)(memory+1400);
        memcpy(io->fnids,io_nids,sizeof(io_nids)); memset(io->funcs,0x5a,120);
        /* Missing one mutation import rejects every patch atomically. */
        io->funcCount=14; memset(stubs,0x5a,80); patch_update_plugin_module(&module);
        for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
        io->funcCount=15;
    }
#endif
    *(unsigned int *)memory=0x27bdfff0; *(unsigned int *)(memory+4)=0xafbf0000;
#ifdef PROX_NATIVE_OTA
    {
        PspModuleImport *paf=(void *)(memory+512+2*sizeof(PspModuleImport));
        module.stub_size=3*sizeof(PspModuleImport)+sizeof(void *); paf->entLen=(sizeof(*paf)+sizeof(void *))/4;
        paf->name=(char *)memory+1800; strcpy((char *)paf->name,"scePaf");
        /* Preserve the real ABI's two variables and extended-header bytes.
         * Host pointers are widened; function/variable counts come from the
         * owner's actual captured 6.61 module, not a zero-variable mock. */
        paf->funcCount=69; paf->varCount=2; paf->fnids=(void *)(memory+1820); paf->funcs=(void *)(memory+2200);
        { void *variables=memory+3000; memcpy((unsigned char *)paf+sizeof(*paf),&variables,sizeof(variables)); }
        memset(memory+3000,0x7b,32);
        for(i=0;i<69;++i) paf->fnids[i]=0x12340000+i;
        paf->fnids[13]=0x3874a5f8; memset(paf->funcs,0x5a,69*8);
        redirect_missing=1; patch_update_plugin_module(&module);
        for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
        for(i=0;i<69*2;++i) assert(paf->funcs[i]==0x5a5a5a5a);
        redirect_missing=0; paf->funcCount=68; patch_update_plugin_module(&module);
        for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
        paf->funcCount=69;
        paf->varCount=0; patch_update_plugin_module(&module);
        for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
        paf->varCount=2;
#ifdef PROX_NATIVE_INSTALL_PREVIEW
        {
            PspModuleImport *gui=(void *)(memory+512+3*sizeof(PspModuleImport)+sizeof(void *));
            module.stub_size=4*sizeof(PspModuleImport)+sizeof(void *);gui->entLen=sizeof(*gui)/4;
            gui->name=(char *)memory+3500;strcpy((char *)gui->name,"sceVshCommonGui");
            gui->funcCount=16;gui->fnids=(void *)(memory+3600);gui->funcs=(void *)(memory+3700);
            for(i=0;i<16;++i) gui->fnids[i]=0x11110000+i;
            gui->fnids[15]=0xf708c5e4;memset(gui->funcs,0x5a,128);
            back_missing=1;patch_update_plugin_module(&module);
            for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
            back_missing=0;gui->funcCount=15;patch_update_plugin_module(&module);
            for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
            gui->funcCount=16;
        }
#endif
        *(unsigned int *)memory=0x27bdfff0; *(unsigned int *)(memory+4)=0xafbf0000;
    }
#endif
    patch_update_plugin_module(&module);
    assert(*(unsigned int *)memory==0x27bdfff0 && *(unsigned int *)(memory+4)==0xafbf0000);
    for(i=0;i<10;++i) assert(stubs[2*i]==0x03e00008 && stubs[2*i+1]==MAKE_SYSCALL(7));
#ifdef PROX_NATIVE_INSTALL_PREVIEW
    {
        unsigned int *events=(void *)(memory+3120);
        assert(completion_guard==1 && events[4]==0x08812400);
        {
            PspModuleImport *gui=(void *)(memory+512+3*sizeof(PspModuleImport)+sizeof(void *));
            assert(gui->funcs[30]==(0x08000000u|((0x08812500u&0x0fffffffu)>>2)) && !gui->funcs[31]);
            for(i=0;i<30;++i) assert(gui->funcs[i]==0x5a5a5a5a);
        }
        memset(stubs,0x5a,80);patch_update_plugin_module(&module);
        assert(completion_guard==1 && events[4]==0x08812400);
        for(i=0;i<10;++i) assert(stubs[2*i]==0x03e00008 && stubs[2*i+1]==MAKE_SYSCALL(7));
        events[4]=module.text_addr+64;
        memset(stubs,0x5a,80);events[5]=1;patch_update_plugin_module(&module);
        assert(!completion_guard && events[4]==module.text_addr+64);
        for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
        events[5]=0;
    }
#endif
#ifdef PROX_NATIVE_OTA
    {
        PspModuleImport *paf=(void *)(memory+512+2*sizeof(PspModuleImport));
        assert(paf->funcs[26]==(0x08000000u|((0x08812340u&0x0fffffffu)>>2)) && !paf->funcs[27]);
        assert(paf->varCount==2);
        { void *variables=NULL; memcpy(&variables,(unsigned char *)paf+sizeof(*paf),sizeof(variables)); assert(variables==memory+3000); }
        for(i=0;i<32;++i) assert(memory[3000+i]==0x7b);
        for(i=0;i<69;++i) if(i!=13) assert(paf->funcs[2*i]==0x5a5a5a5a);
    }
#endif
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
    {
        PspModuleImport *io=(void *)(memory+512+sizeof(PspModuleImport));
        for(i=0;i<15;++i) {
            unsigned int expected_word=prox_native_preview_io_function(io->fnids[i])?0x03e00008:0x5a5a5a5a;
            assert(io->funcs[2*i]==expected_word);
        }
        memset(stubs,0x5a,80); io->fnids[0]=0x12345678;
        patch_update_plugin_module(&module);
        for(i=0;i<20;++i) assert(stubs[i]==0x5a5a5a5a);
        io->fnids[0]=0x42ec03ac;
    }
#endif
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
