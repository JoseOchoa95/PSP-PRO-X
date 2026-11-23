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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
static unsigned int completion_patched_slot,completion_patched_handler,completion_patched_text;
#include "native_completion_preview.h"
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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
/* Captured native 6.61 event triples: name, handler, reserved. Require the
 * adjacent named events and original handler prologue before any mutation. */
static int event_name(SceModule2 *mod,unsigned int address,const char *expected)
{
    unsigned int n=(unsigned int)strlen(expected)+1;
    return in_module(mod,address,n) && !memcmp((const void *)address,expected,n);
}
static unsigned int completion_button_slot(SceModule2 *mod)
{
    unsigned int segment,found=0,hits=0;
    if(mod->nsegment>4 || mod->text_size<8) return 0;
    for(segment=0;segment<mod->nsegment;++segment) {
        unsigned int offset,base=mod->segmentaddr[segment],size=mod->segmentsize[segment];
        if((base&3u) || size>2u*1024u*1024u || size<36 || base>0xffffffffu-size) continue;
        for(offset=12;offset<=size-24;offset+=4) {
            unsigned int *p=(void *)(base+offset),handler=p[1];
            if(!event_name(mod,p[0],"onPushNetworkDownloadComplete")) continue;
            /* The existing linker hook reapplies imports after libupdown is
             * linked. Recognize only our own recorded patch, then revalidate
             * the original function rather than rejecting that second pass. */
            if((unsigned int)(p+1)==completion_patched_slot && mod->text_addr==completion_patched_text &&
               handler==prox_native_completion_noop_address()) handler=completion_patched_handler;
            if(p[2] || p[-1] || p[5] || !event_name(mod,p[-3],"onCancelNoNetworkUpdate") ||
               !event_name(mod,p[3],"onInitNetworkDownload") || (handler&3u) ||
               handler<mod->text_addr || handler-mod->text_addr>mod->text_size-8 || !in_module(mod,handler,8) ||
               *(unsigned int *)handler!=0x27bdff20u || *(unsigned int *)(handler+4)!=0xafbf00dcu) return 0;
            found=(unsigned int)(p+1);++hits;
        }
    }
    return hits==1?found:0;
}
#endif
int prox_hook_native_probe_imports(SceModule2 *mod)
{
    unsigned int offset=0,sites[32],calls[32],count=0,provider_count=0;
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    unsigned int completion_slot=0,completion_noop=0;
    unsigned int back_count=0,back_site=0,back_redirect=0,gui_count=0;
    prox_native_completion_guard(0);
    completion_slot=completion_button_slot(mod);
    if(!completion_slot) return -1;
#endif
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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
        if(table->name && in_module(mod,(unsigned int)table->name,16) && !memcmp(table->name,"sceVshCommonGui",16)) {
            /* Captured 6.61 update_plugin: 16 functions, no variables. Hook
             * only the named native Back visibility API, never its renderer. */
            if(gui_count++ || table->funcCount!=16 || table->varCount ||
               ((unsigned int)table->fnids&3) || ((unsigned int)table->funcs&3) ||
               !in_module(mod,(unsigned int)table->fnids,64) || !in_module(mod,(unsigned int)table->funcs,128)) return -1;
            for(j=0;j<16;++j) {
                unsigned int k;
                for(k=0;k<j;++k) if(table->fnids[k]==table->fnids[j]) return -1;
                if(table->fnids[j]!=0xf708c5e4) continue;
                if(back_count++ || count>=32) return -1;
                back_site=count;calls[count]=0;sites[count++]=(unsigned int)&table->funcs[j*2];
            }
        }
#endif
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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    if(io_seen!=1 || io_count!=10 || count!=22 || paf_count!=1 || gui_count!=1 || back_count!=1) return -1;
#else
    if(io_seen!=1 || io_count!=10 || count!=21 || paf_count!=1) return -1;
#endif
    redirect=prox_native_error_prepare();
    if(redirect<0x08800000u || redirect>=0x0c000000u || (redirect&3u)) return -1;
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    completion_noop=prox_native_completion_noop_address();
    if(completion_noop<0x08800000u || completion_noop>=0x0c000000u || (completion_noop&3u)) return -1;
    back_redirect=prox_native_completion_back_address();
    if(back_redirect<0x08800000u || back_redirect>=0x0c000000u || (back_redirect&3u)) return -1;
#endif
#else
    if(io_seen!=1 || io_count!=10 || count!=20) return -1;
#endif
#else
    if(io_seen!=1 || io_count!=9 || count!=19) return -1;
#endif
#endif
    for(offset=0;offset<count;++offset) {
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
        if(offset==back_site) {
            _sw(0x08000000u|((back_redirect&0x0fffffffu)>>2),sites[offset]);_sw(0,sites[offset]+4);continue;
        }
#endif
#ifdef PROX_NATIVE_OTA
        if(offset==paf_site) {
            _sw(0x08000000u|((redirect&0x0fffffffu)>>2),sites[offset]); _sw(0,sites[offset]+4); continue;
        }
#endif
        _sw(0x03e00008,sites[offset]);
        _sw(MAKE_SYSCALL(calls[offset]),sites[offset]+4);
    }
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    if(*(unsigned int *)completion_slot!=completion_noop) {
        completion_patched_slot=completion_slot;
        completion_patched_handler=*(unsigned int *)completion_slot;
        completion_patched_text=mod->text_addr;
    }
    _sw(completion_noop,completion_slot);
    prox_native_completion_guard(1);
#endif
    return 0;
}
