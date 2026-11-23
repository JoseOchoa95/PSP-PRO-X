/* Redirect local prerequisite failures through the original scePafGetText.
 * Keep resource ownership, localization, theme and rendering in Sony's UI.
 * User code tail-calls the actual user PAF function; never call it from kernel.
 * GPL-3.0-or-later. */
#include "native_error_text.h"
#include "prox_native_query.h"
#include <string.h>
const char *prox_native_error_select(unsigned int failure,const char *key,
                                     const char *storage,const char *generic)
{
    if(!key || (strncmp(key,"msgupdate_error_",16) && strncmp(key,"msgshare_error_",15))) return key;
    if(failure==PROX_NQ_CAUSE_STORAGE) return storage;
    if(failure==PROX_NQ_CAUSE_POWER || failure==PROX_NQ_CAUSE_CLIENT) return generic;
    return key; /* Genuine network/TLS/HTTP errors keep the system's error. */
}
int prox_native_error_code(unsigned int *out,size_t count,uintptr_t original,unsigned int syscall,uintptr_t address)
{
    unsigned int code[]={
        0x27bdffe0,0xafbf001c,0xafbc0018,0xafa40014, /* stack, RA, GP, resource */
        0x00a02021,0,0,0x00402821,0x8fa40014,       /* key -> standard stub -> new key */
        0x8fbc0018,0x8fbf001c,0,0,0x03200008,0x27bd0020,0,
        0x03e00008,0                             /* standard jr-ra / syscall stub */
    };
    if(!out || count<18 || original<0x08800000u || original>=0x0c000000u ||
       (original&3u) || syscall>0xfffffu || address<0x08800000u ||
       address>0x0c000000u-sizeof(code) || (address&3u)) return -1;
    code[5]=0x0c000000u|(((unsigned int)address+16*4)>>2);
    code[17]=(syscall<<6)|12u;
    code[11]=0x3c190000u|((unsigned int)(original>>16)&0xffffu);
    code[12]=0x37390000u|((unsigned int)original&0xffffu);
    memcpy(out,code,sizeof(code)); return 0;
}
int prox_native_notice_code(unsigned int *out,size_t count,uintptr_t original,unsigned int key_syscall,unsigned int notice_syscall,uintptr_t address)
{
    unsigned int code[]={
        0x27bdffe0,0xafbf001c,0xafbc0018,0xafa40014,0xafa50010,
        0x00a02021,0,0,0x10400005,0,
        0x8fbc0018,0x8fbf001c,0x03e00008,0x27bd0020,
        0x8fa40010,0,0,0x00402821,0x8fa40014,0x8fbc0018,0x8fbf001c,
        0,0,0x03200008,0x27bd0020,0,0,0,
        0x03e00008,0,0x03e00008,0,0x03e00008,0x00001021
    };
    if(!out || count<34 || original<0x08800000u || original>=0x0c000000u || (original&3u) ||
       key_syscall>0xfffffu || notice_syscall>0xfffffu || address<0x08800000u ||
       address>0x0c000000u-sizeof(code) || (address&3u)) return -1;
    code[6]=0x0c000000u|(((unsigned int)address+30*4)>>2);
    code[15]=0x0c000000u|(((unsigned int)address+28*4)>>2);
    code[21]=0x3c190000u|((unsigned int)(original>>16)&0xffffu);
    code[22]=0x37390000u|((unsigned int)original&0xffffu);
    code[29]=(key_syscall<<6)|12u;code[31]=(notice_syscall<<6)|12u;
    memcpy(out,code,sizeof(code));return 0;
}
/* One-boolean user-mode bridge. Select visibility in kernel, then tail-call
 * the actual user-mode native ShowButtonNavigation_Back(bool) export. */
int prox_native_back_code(unsigned int *out,size_t count,uintptr_t original,unsigned int syscall,uintptr_t address)
{
    /* Keep the ABI's four argument home slots separate from saved RA/GP. */
    unsigned int code[]={0x27bdffe0,0xafbf001c,0xafbc0018,0,0,0x00402021,
        0x8fbc0018,0x8fbf001c,0,0,0x03200008,0x27bd0020,0x03e00008,0};
    if(!out || count<14 || original<0x08800000u || original>=0x0c000000u || (original&3u) ||
       syscall>0xfffffu || address<0x08800000u || address>0x0c000000u-sizeof(code) || (address&3u)) return -1;
    code[3]=0x0c000000u|(((unsigned int)address+12*4)>>2);
    code[8]=0x3c190000u|((unsigned int)(original>>16)&0xffffu);
    code[9]=0x37390000u|((unsigned int)original&0xffffu);code[13]=(syscall<<6)|12u;
    memcpy(out,code,sizeof(code));return 0;
}
#ifndef PROX_ERROR_TEXT_NATIVE
#include <pspkernel.h>
#include <pspsdk.h>
#include "systemctrl.h"
#include "native_query_provider.h"
#include "native_completion_preview.h"
typedef struct { unsigned int code[34],back_code[14]; char storage[32],generic[32]; unsigned short notice[PROX_COMPLETION_UNITS]; } UserBridge;
static UserBridge *user;
static int block_id=-1;
static unsigned int paf_original;
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
static unsigned int back_original;
#endif
unsigned int prox_native_error_prepare(void)
{
    typedef int (*Alloc)(int,const char *,int,unsigned int,void *);
    typedef void *(*Head)(int);
    typedef int (*Free)(int);
    Alloc allocate; Head head; Free release; SceModule2 *paf;
    unsigned int original,syscall; int block,k1,rc; UserBridge *candidate;
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    SceModule2 *gui=sceKernelFindModuleByName("sceVshCommonGui_Module");
    unsigned int back=sctrlHENFindFunction("sceVshCommonGui_Module","sceVshCommonGui",0xf708c5e4);
    unsigned int back_syscall=sctrlKernelQuerySystemCall(proxNativeCompletionBack);
    if(!gui || !back || gui->text_size<4 || back<gui->text_addr ||
       back-gui->text_addr>gui->text_size-4 || back_syscall>0xfffffu) return 0;
#endif
    paf=sceKernelFindModuleByName("scePaf_Module");
    original=sctrlHENFindFunction("scePaf_Module","scePaf",0x3874a5f8);
    if(!paf || !original || paf->text_size<4 || original<paf->text_addr ||
       original-paf->text_addr>paf->text_size-4) {
        prox_native_query_event("native error bridge: PAF export unavailable",-1); return 0;
    }
    syscall=sctrlKernelQuerySystemCall(proxNativeQueryTextKey);
    if(syscall>0xfffffu) { prox_native_query_event("native error bridge: syscall unavailable",-1); return 0; }
    if(user) {
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
        if(back_original!=back) return 0;
#endif
        if(paf_original!=original) prox_native_query_event("native error bridge: PAF address changed",-1);
        return paf_original==original?(unsigned int)user->code:0;
    }
    allocate=(Alloc)sctrlHENFindFunction("sceSystemMemoryManager","SysMemForKernel",0x237dbd4f);
    head=(Head)sctrlHENFindFunction("sceSystemMemoryManager","SysMemForKernel",0x9d9a5ba1);
    release=(Free)sctrlHENFindFunction("sceSystemMemoryManager","SysMemForKernel",0xb6d61d02);
    if(!allocate || !head || !release) { prox_native_query_event("native error bridge: SysMem unavailable",-1); return 0; }
    k1=pspSdkSetK1(0); block=allocate(2,"PRO-X native errors",PSP_SMEM_High,sizeof(*candidate),NULL);
    if(block<0) { pspSdkSetK1(k1); prox_native_query_event("native error bridge: allocation failed",block); return 0; }
    candidate=head(block);
    if((uintptr_t)candidate<0x08800000u || (uintptr_t)candidate>0x0c000000u-sizeof(*candidate) || ((uintptr_t)candidate&3u)) {
        release(block);pspSdkSetK1(k1);return 0;
    }
    memset(candidate,0,sizeof(*candidate));
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    rc=prox_native_notice_code(candidate->code,34,original,syscall,
           sctrlKernelQuerySystemCall(proxNativeCompletionText),(uintptr_t)candidate);
    if(!rc) rc=prox_native_back_code(candidate->back_code,14,back,back_syscall,(uintptr_t)candidate->back_code);
#else
    rc=prox_native_error_code(candidate->code,34,original,syscall,(uintptr_t)candidate);
#endif
    if(rc) {
        release(block); pspSdkSetK1(k1); prox_native_query_event("native error bridge: user memory rejected",-1); return 0;
    }
    strcpy(candidate->storage,"msgshare_error_notplay_ms");
    strcpy(candidate->generic,"msgupdate_error_start");
    /* One bounded user block lives with VSH, just like the product label.
     * Native consumers may retain returned key pointers. Never free/reuse it. */
    block_id=block; user=candidate; paf_original=original;
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    back_original=back;
#endif
    prox_native_completion_buffer(candidate->notice);
    sceKernelDcacheWritebackInvalidateAll(); sceKernelIcacheInvalidateAll();
    pspSdkSetK1(k1); prox_native_query_event("native error bridge ready",0); return (unsigned int)user->code;
}
const char *prox_native_error_redirect(unsigned int failure,const char *key)
{
    uintptr_t address=(uintptr_t)key; unsigned int i;
    if(!user || block_id<0 || !key || address<0x08800000u || address>0x0c000000u-64) return key;
    for(i=0;i<64;++i) if(!key[i]) break;
    if(i==64) return key;
    return prox_native_error_select(failure,key,user->storage,user->generic);
}
unsigned int prox_native_completion_noop_address(void)
{
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    return user && block_id>=0?(unsigned int)(user->code+32):0;
#else
    return 0;
#endif
}
unsigned int prox_native_completion_back_address(void)
{
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    return user && block_id>=0 && back_original?(unsigned int)user->back_code:0;
#else
    return 0;
#endif
}
#endif
