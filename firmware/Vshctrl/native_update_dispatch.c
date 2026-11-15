/* Original-UI diagnostic import adapter, GPL-3.0-or-later. No Sony code or
 * offsets; all known provider imports must be intercepted before any write. */
#ifdef PROX_UPDATE_NATIVE
#include "update_dispatch_shim.h"
#else
#include <pspkernel.h>
#include <pspsdk.h>
#include <string.h>
#include "systemctrl.h"
#include "utils.h"
#include "libs.h"
#include "main.h"
#endif
#include "native_update_probe.h"
static int in_module(SceModule2 *mod,unsigned int address,unsigned int size)
{
    unsigned int i;
    if(!size || address+size<address || mod->nsegment>4) return 0;
    for(i=0;i<mod->nsegment;++i)
        if(address>=mod->segmentaddr[i] && size<=mod->segmentsize[i] &&
           address-mod->segmentaddr[i]<=mod->segmentsize[i]-size) return 1;
    return 0;
}
int prox_hook_native_probe_imports(SceModule2 *mod)
{
    unsigned int offset=0,sites[10],calls[10],count=0;
    if(!conf.useownupdate || ((unsigned int)mod->stub_top&3) || mod->stub_size>16384 ||
       !in_module(mod,(unsigned int)mod->stub_top,mod->stub_size)) return -1;
    while(offset<mod->stub_size) {
        PspModuleImport *table=(void *)((unsigned int)mod->stub_top+offset);
        unsigned int bytes,j;
        if(mod->stub_size-offset<20) return -1;
        bytes=table->entLen*4;
        if(bytes<20 || bytes>mod->stub_size-offset) return -1;
        if(table->name && in_module(mod,(unsigned int)table->name,15) &&
           !memcmp(table->name,"sceLibUpdateDL",15)) {
            if(table->funcCount!=10 || table->varCount || count ||
               ((unsigned int)table->fnids&3) || ((unsigned int)table->funcs&3) ||
               !in_module(mod,(unsigned int)table->fnids,40) ||
               !in_module(mod,(unsigned int)table->funcs,80)) return -1;
            for(j=0;j<10;++j) {
                unsigned int k,nid=table->fnids[j];
                void *function=prox_native_probe_function(nid);
                if(!function) return -1;
                for(k=0;k<j;++k) if(table->fnids[k]==nid) return -1;
                calls[count]=sctrlKernelQuerySystemCall(function);
                if(calls[count]==(unsigned int)-1 || calls[count]>0xfffff) return -1;
                sites[count++]=(unsigned int)&table->funcs[j*2];
            }
        }
        offset+=bytes;
    }
    if(count!=10) return -1;
    for(offset=0;offset<count;++offset) {
        _sw(0x03e00008,sites[offset]);
        _sw(MAKE_SYSCALL(calls[offset]),sites[offset]+4);
    }
    return 0;
}
