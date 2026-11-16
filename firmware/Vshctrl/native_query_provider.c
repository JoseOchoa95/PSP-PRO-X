/* Native frontend RPC adapter. Network work remains in its user-mode owner.
 * No retained user pointers, original HTTP or installer fallback. GPL-3.0-or-later. */
#ifdef PROX_NQ_PROVIDER_TEST
#include "native_query_provider_shim.h"
#else
#include <pspkernel.h>
#include <pspsdk.h>
#include "systemctrl.h"
#endif
#include <string.h>
#include "native_query_provider.h"
#include "native_query_state.h"
#ifdef PROX_NATIVE_OTA
#include "native_error_text.h"
#endif
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
#include "native_offer_preview.h"
#endif
#define NATIVE_CURRENT ((int)0x805f0004)
#define NATIVE_ERROR ((int)0x805f0007)
#define NATIVE_STATE ((int)0x805f0002)
#define NATIVE_ARGUMENT ((int)0x805f000b)
static ProxNativeQueryState state;
#ifdef PROX_NATIVE_OTA
static unsigned int transfer_started,transfer_generation;
#endif
static int valid(const void *p,unsigned int size)
{
#ifdef PROX_NQ_PROVIDER_TEST
    return prox_native_test_valid(p,size);
#else
    unsigned int address=(unsigned int)p;
    return size && !(address&3) && address>=0x08800000 && size<=0x10000 && address<=0x0c000000-size;
#endif
}
int proxNativeQueryService(int action,ProxNativeQueryMessage *packet,unsigned int size)
{
    ProxNativeQueryMessage message; unsigned int mask; int rc,thread,k1;
    if(size!=sizeof(message) || !valid(packet,size)) return -1;
    k1=pspSdkSetK1(0); memcpy(&message,packet,sizeof(message)); thread=sceKernelGetThreadId();
    mask=pspSdkDisableInterrupts(); rc=prox_nq_exchange(&state,thread,action,&message); pspSdkEnableInterrupts(mask);
    if(!rc) memcpy(packet,&message,sizeof(message));
    pspSdkSetK1(k1); return rc;
}
void prox_native_query_load_failed(void)
{ prox_native_query_load_failed_reason(PROX_NQ_CAUSE_CLIENT); }
void prox_native_query_load_failed_reason(unsigned int failure)
{
    unsigned int mask=pspSdkDisableInterrupts();
    state.fault=1; state.cancel=1; state.done=1; state.answer.result=-1;
    state.answer.failure=failure<=PROX_NQ_CAUSE_CLIENT?failure:PROX_NQ_CAUSE_CLIENT;
    pspSdkEnableInterrupts(mask);
}
const char *proxNativeQueryTextKey(const char *key)
{
#ifdef PROX_NATIVE_OTA
    unsigned int mask=pspSdkDisableInterrupts(),failure=state.answer.failure;
    pspSdkEnableInterrupts(mask);
    return prox_native_error_redirect(failure,key);
#else
    return key;
#endif
}
int proxNativeQueryInitEx(const void *allocate,const void *release)
{
    unsigned int mask,lease,seed; int rc,k1,loaded,loading;
    if(!allocate || !release) return NATIVE_ARGUMENT;
    k1=pspSdkSetK1(0);
    seed=sceKernelGetSystemTimeLow()|1u;
    loaded=sceKernelFindModuleByName("ProxUpdater")!=NULL; loading=prox_native_query_is_loading();
    mask=pspSdkDisableInterrupts();
    if(!state.lease || (!loaded && !loading && !state.active)) prox_nq_reset(&state,seed);
    rc=prox_nq_open(&state); lease=state.lease;
    pspSdkEnableInterrupts(mask);
    if(!rc) rc=prox_native_query_start(lease);
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
    if(!rc) prox_native_preview_reset();
#endif
    if(rc<0) prox_native_query_load_failed();
    prox_native_query_event("native query InitEx",rc);
    pspSdkSetK1(k1); return rc<0?NATIVE_ERROR:0;
}
int proxNativeQuerySetting(unsigned int ignored) { (void)ignored; return 0; }
int proxNativeQueryAbort(void)
{
    unsigned int mask=pspSdkDisableInterrupts(); prox_nq_cancel(&state); pspSdkEnableInterrupts(mask);
#if defined(PROX_NATIVE_OFFER_PREVIEW) || defined(PROX_NATIVE_OTA)
    prox_native_preview_abort();
#endif
    prox_native_query_event("native query Abort",0); return 0;
}
int proxNativeQueryEnd(void)
{ proxNativeQueryAbort(); prox_native_query_event("native query End",0); return 0; }
int proxNativeQueryConnect(unsigned int *size,unsigned int *detail)
{
    unsigned int start=sceKernelGetSystemTimeLow(),generation=0,mask;
#ifdef PROX_NATIVE_OTA
    unsigned int package_size;
#endif
    int ready,fault,cancel,active,done,rc,result;
    if(!valid(size,4) || !valid(detail,4)) return NATIVE_ARGUMENT;
    *size=0; *detail=0;
    for(;;) {
        mask=pspSdkDisableInterrupts(); ready=state.ready; fault=state.fault; cancel=state.cancel;
        generation=state.generation; pspSdkEnableInterrupts(mask);
        if(fault || cancel) return NATIVE_ERROR;
        if(ready) break;
        if((unsigned int)(sceKernelGetSystemTimeLow()-start)>10000000u) {
            prox_native_query_event("native query worker startup timeout",NATIVE_ERROR); return NATIVE_ERROR;
        }
        sceKernelDelayThread(10000);
    }
    mask=pspSdkDisableInterrupts(); rc=prox_nq_submit(&state); pspSdkEnableInterrupts(mask);
    if(rc) return NATIVE_STATE;
    for(;;) {
        mask=pspSdkDisableInterrupts(); done=state.done; fault=state.fault; cancel=state.cancel;
        active=state.active; result=state.answer.result;
#ifdef PROX_NATIVE_OTA
        package_size=state.answer.package_size;
#endif
        if(state.generation!=generation) fault=1;
        pspSdkEnableInterrupts(mask);
        if(fault || cancel) return NATIVE_ERROR;
        if(done && !active) break;
        if((unsigned int)(sceKernelGetSystemTimeLow()-start)>120000000u) {
            proxNativeQueryAbort(); prox_native_query_event("native query total deadline",NATIVE_ERROR); return NATIVE_ERROR;
        }
        sceKernelDelayThread(10000);
    }
#ifdef PROX_NATIVE_OTA
    if(result==1 && package_size>=65536 && package_size<=16u*1024u*1024u) {
        *size=package_size; prox_native_preview_offer(package_size);
        prox_native_query_event("native REAL compatible offer; awaiting consent",0); return 0;
    }
#endif
    if(result==0) {
#ifdef PROX_NATIVE_OFFER_PREVIEW
        *size=1024u*1024u;
        prox_native_preview_offer(*size);
        prox_native_query_event("PREVIEW simulated offer; native writes blocked",0); return 0;
#else
        prox_native_query_event("native query REAL current",NATIVE_CURRENT); return NATIVE_CURRENT;
#endif
    }
    /* A positive target is not a false current verdict and cannot enter the
     * original unverified package writer. Native offer/install is a later step. */
    prox_native_query_event(result==1?"native query newer target; handoff blocked":"native query failed",NATIVE_ERROR);
    return NATIVE_ERROR;
}
int proxNativeQueryRead(void *buffer,unsigned int capacity,unsigned int *detail)
{
    (void)buffer; (void)capacity;
    if(!valid(detail,4)) return NATIVE_ARGUMENT;
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    {
        int rc;
        *detail=0; rc=prox_native_preview_read(buffer,capacity);
        /* sceLibUpdateDL has its own error domain. The stock caller does not
         * classify arbitrary IoFileMgr errors as failed downloads. Never pass
         * an incomplete EOF, raw filesystem error or impossible byte count. */
        if(rc<=0 || (unsigned int)rc>capacity) {
            prox_native_query_event("native progress read failed",NATIVE_ERROR); return NATIVE_ERROR;
        }
        return rc;
    }
#else
    *detail=0; prox_native_query_event("native query package read blocked",NATIVE_STATE); return NATIVE_STATE;
#endif
}
#ifdef PROX_NATIVE_OTA
int prox_native_ota_begin(int owner,unsigned int total)
{
    unsigned int mask,start=sceKernelGetSystemTimeLow(); int rc;
    mask=pspSdkDisableInterrupts();
    rc=total==state.answer.package_size?prox_nq_download(&state,owner):-1;
    if(!rc) { transfer_started=start; transfer_generation=state.generation; }
    pspSdkEnableInterrupts(mask);
    prox_native_query_event("native consent starts REAL private download",rc); return rc;
}
int prox_native_ota_wait(int owner,unsigned int received,unsigned int wanted)
{
    for(;;) {
        unsigned int mask=pspSdkDisableInterrupts(); int rc=-1,launch=0;
        if(state.active && !state.cancel && !state.fault && state.consumer==owner &&
           owner>0 && state.generation==transfer_generation && received<=state.answer.package_size &&
           wanted<=state.answer.package_size-received) {
            if(wanted && state.phase!=PROX_NQ_HANDOFF && state.downloaded>=received+wanted) rc=0;
            else if(!wanted && state.phase==PROX_NQ_VERIFIED) {
                launch=prox_nq_handoff(&state,owner)==0; rc=launch?1:-1;
            } else if(state.phase==PROX_NQ_TRANSFER || state.phase==PROX_NQ_HANDOFF) rc=1;
        }
        pspSdkEnableInterrupts(mask);
        if(launch) prox_native_query_event("native verified stage; launching target installer",0);
        if(rc<=0) return rc;
        if((unsigned int)(sceKernelGetSystemTimeLow()-transfer_started)>300000000u) {
            proxNativeQueryAbort(); return -1;
        }
        sceKernelDelayThread(10000);
    }
}
#endif
void *prox_native_query_function(unsigned int nid)
{
    switch(nid) {
    case 0x34243b86: case 0x36d8f34b: case 0x88ff3935: case 0xb5db018d: case 0xc1af1076: return proxNativeQuerySetting;
    case 0x4f49c9c1: return proxNativeQueryAbort;
    case 0xd6a09757: return proxNativeQueryEnd;
    case 0xf6690a9a: return proxNativeQueryInitEx;
    case 0xfa9aa797: return proxNativeQueryRead;
    case 0xfd675e8d: return proxNativeQueryConnect;
    default: return 0;
    }
}
