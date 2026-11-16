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
#ifdef PROX_NATIVE_UI_QUERY
#include "native_query_provider.h"
#define prox_native_probe_function prox_native_query_function
#endif
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
#include "native_offer_preview.h"
#endif
#ifdef PROX_NATIVE_OTA
#include "native_error_text.h"
#endif
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
    unsigned int offset=0,sites[32],calls[32],count=0,provider_count=0;
#ifdef PROX_NATIVE_OTA
    unsigned int paf_count=0,paf_site=0,redirect;
#endif
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
    unsigned int io_count=0,io_seen=0;
#endif
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
            if(table->funcCount!=10 || table->varCount || provider_count ||
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
                ++provider_count;
            }
        }
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
        if(table->name && in_module(mod,(unsigned int)table->name,17) &&
           !memcmp(table->name,"IoFileMgrForUser",17)) {
            if(io_seen++ || table->funcCount!=15 || table->varCount ||
               ((unsigned int)table->fnids&3) || ((unsigned int)table->funcs&3) ||
               !in_module(mod,(unsigned int)table->fnids,60) || !in_module(mod,(unsigned int)table->funcs,120)) return -1;
            for(j=0;j<15;++j) {
                unsigned int k,nid=table->fnids[j]; void *function;
                if(!prox_native_preview_io_known(nid)) return -1;
                for(k=0;k<j;++k) if(table->fnids[k]==nid) return -1;
                function=prox_native_preview_io_function(nid);
                if(!function) continue;
                if(count>=32) return -1;
                calls[count]=sctrlKernelQuerySystemCall(function);
                if(calls[count]==(unsigned int)-1 || calls[count]>0xfffff) return -1;
                sites[count++]=(unsigned int)&table->funcs[j*2]; ++io_count;
            }
        }
#endif
#ifdef PROX_NATIVE_OTA
        if(table->name && in_module(mod,(unsigned int)table->name,7) && !memcmp(table->name,"scePaf",7)) {
            /* Captured 6.61 header: six words, 69 function imports and two
             * legitimate variable imports. Leave their extension untouched. */
            if(table->funcCount!=69 || table->varCount!=2 || bytes<24 ||
               ((unsigned int)table->fnids&3) || ((unsigned int)table->funcs&3) ||
               !in_module(mod,(unsigned int)table->fnids,69*4) || !in_module(mod,(unsigned int)table->funcs,69*8)) return -1;
            for(j=0;j<69;++j) {
                unsigned int k;
                for(k=0;k<j;++k) if(table->fnids[k]==table->fnids[j]) return -1;
                if(table->fnids[j]!=0x3874a5f8) continue;
                if(paf_count++ || count>=32) return -1;
                paf_site=count; calls[count]=0; sites[count++]=(unsigned int)&table->funcs[j*2];
            }
        }
#endif
        offset+=bytes;
    }
    if(provider_count!=10) return -1;
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
#ifdef PROX_NATIVE_OTA
    if(io_seen!=1 || io_count!=10 || count!=21 || paf_count!=1) return -1;
    redirect=prox_native_error_prepare();
    if(redirect<0x08800000u || redirect>=0x0c000000u || (redirect&3u)) return -1;
#else
    if(io_seen!=1 || io_count!=10 || count!=20) return -1;
#endif
#else
    if(io_seen!=1 || io_count!=9 || count!=19) return -1;
#endif
#endif
    for(offset=0;offset<count;++offset) {
#ifdef PROX_NATIVE_OTA
        if(offset==paf_site) {
            _sw(0x08000000u|((redirect&0x0fffffffu)>>2),sites[offset]); _sw(0,sites[offset]+4); continue;
        }
#endif
        _sw(0x03e00008,sites[offset]);
        _sw(MAKE_SYSCALL(calls[offset]),sites[offset]+4);
    }
    return 0;
}
