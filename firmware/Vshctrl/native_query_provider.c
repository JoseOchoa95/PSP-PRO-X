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
#include "prox_native_install.h"
#include "prox_product.h"
#include "prox_dedicated_storage.h"
#ifdef PROX_NATIVE_MOUNT_ACCESS
#include "native_mount_access.h"
#include "native_flash_preflight.h"
#ifdef PROX_NATIVE_DEDICATED_OTA
#include "native_dedicated_cleanup.h"
#endif
#ifndef PROX_NQ_PROVIDER_TEST
#include <pspthreadman_kernel.h>
#endif
static int access_busy;
#endif
#ifdef PROX_NATIVE_BACKGROUND
#include "native_install_state.h"
#endif
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
#include "prox_install_preview.h"
#include "native_completion_preview.h"
int prox_native_install_preview_locked(void)
{
#ifdef PROX_NATIVE_BACKGROUND
    return prox_native_install_critical();
#else
    static unsigned int started;
    int loaded=sceKernelFindModuleByName(PROX_INSTALL_PREVIEW_MODULE)!=NULL;
    unsigned int now=sceKernelGetSystemTimeLow(),generation=prox_native_completion_generation();
    unsigned int mask=pspSdkDisableInterrupts();
    int holding=loaded && prox_native_completion_health(generation,now)==2;
    pspSdkEnableInterrupts(mask);
    return holding || prox_install_preview_lock(now,&started,loaded);
#endif
}
#endif
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
#ifdef PROX_NATIVE_BACKGROUND
static ProxNativeInstallState install_state;
int prox_native_install_critical(void)
{
    unsigned int mask=pspSdkDisableInterrupts();int locked=prox_ni_locked(&install_state);
#if defined(PROX_NATIVE_MOUNT_ACCESS) && defined(PROX_NATIVE_DEDICATED_OTA)
    locked=locked || access_busy;
#endif
    pspSdkEnableInterrupts(mask);return locked;
}
int prox_native_install_completion_owner(int thread,unsigned int generation)
{
    unsigned int mask=pspSdkDisableInterrupts();
    int ok=generation && install_state.generation==generation && install_state.engine==thread &&
           install_state.phase==PROX_NI_COMMITTED;
    pspSdkEnableInterrupts(mask);return ok;
}
#endif
static int __attribute__((noinline)) install_service_inner(int action,ProxNativeInstallMessage *packet,unsigned int size)
{
#ifdef PROX_NATIVE_BACKGROUND
    ProxNativeInstallMessage message;unsigned int mask,now;int thread,k1,rc=-1,loaded=-1;
    if(size!=sizeof(message) || !packet) return -1;
#ifdef PROX_NQ_PROVIDER_TEST
    if(!prox_native_test_valid(packet,size)) return -1;
    { extern int prox_native_install_test_module(void);loaded=prox_native_install_test_module(); }
#else
    {
        unsigned int address=(unsigned int)packet;SceModule2 *module=sceKernelFindModuleByName(PROX_NI_MODULE);
        if((address&3u) || address<0x08800000u || address>0x0c000000u-size) return -1;
        if(module) loaded=module->modid;
    }
#endif
    k1=pspSdkSetK1(0);memcpy(&message,packet,sizeof(message));thread=sceKernelGetThreadId();now=sceKernelGetSystemTimeLow();
#ifdef PROX_NATIVE_MOUNT_ACCESS
    if(action==PROX_NI_ACCESS_PROBE || action==PROX_NI_ACCESS_CAPS || action==PROX_NI_ACCESS_BEGIN || action==PROX_NI_ACCESS_END) {
        int cleanup=action==PROX_NI_ACCESS_END;
        mask=pspSdkDisableInterrupts();
        rc=message.protocol==PROX_NI_PROTOCOL &&
           (cleanup || (message.lease==state.lease && message.generation==state.generation &&
            state.active && !state.cancel && !state.fault && state.phase==PROX_NQ_HANDOFF)) &&
           thread>0 && thread==install_state.engine &&
           (install_state.phase==(unsigned int)(action==PROX_NI_ACCESS_CAPS?PROX_NI_CLAIMED:PROX_NI_WRITING) ||
            (cleanup && install_state.phase==PROX_NI_FAILED)) &&
           message.module>0 && message.module==loaded &&
           loaded==install_state.module && install_state.lease==message.lease &&
           install_state.generation==message.generation && !access_busy?0:-1;
        if(!rc && action==PROX_NI_ACCESS_CAPS && prox_native_mount_active())rc=-1;
        if(!rc && action!=PROX_NI_ACCESS_CAPS)access_busy=1;
        pspSdkEnableInterrupts(mask);
        if(action==PROX_NI_ACCESS_CAPS) {
            if(!rc){message.result=PROX_NI_ACCESS_REVISION;memcpy(packet,&message,sizeof(message));}
            pspSdkSetK1(k1);return rc;
        }
        /* Mutex waits and raw descriptor operations must NEVER run with
         * interrupts disabled. Only the bound engine's copied job can enter. */
        if(!rc) {
            memset(&message.access,0,sizeof(message.access));
            if(action==PROX_NI_ACCESS_BEGIN) {
                message.result=0;
                if(!install_state.flash_preflight_passed) {
#ifdef PROX_NATIVE_DEDICATED_OTA
                    message.result=prox_native_flash_preflight_dedicated(install_state.job.parts,&install_state.resident);
#else
                    message.result=prox_native_flash_preflight(install_state.job.parts);
#endif
                    mask=pspSdkDisableInterrupts();
                    if(!message.result &&
                       (state.cancel || state.fault || !state.active || state.phase!=PROX_NQ_HANDOFF ||
                        state.lease!=message.lease || state.generation!=message.generation))
                        message.result=-1;
                    pspSdkEnableInterrupts(mask);
                }
                if(!message.result) {
                    message.result=prox_native_mount_begin(&message.access);
                    if(!message.result) install_state.flash_preflight_passed=1;
                }
            }
            else if(action==PROX_NI_ACCESS_END)message.result=prox_native_mount_end(&message.access);
            else message.result=prox_native_mount_access_probe(&message.access);
            mask=pspSdkDisableInterrupts();access_busy=0;pspSdkEnableInterrupts(mask);
            memcpy(packet,&message,sizeof(message));
        }
        pspSdkSetK1(k1);return rc;
    }
#endif
    mask=pspSdkDisableInterrupts();
#ifdef PROX_NATIVE_MOUNT_ACCESS
    if(action==PROX_NI_FAIL && message.protocol==PROX_NI_PROTOCOL && !access_busy && thread>0 &&
       thread==install_state.engine && message.module==install_state.module && loaded==message.module &&
       message.lease==install_state.lease && message.generation==install_state.generation) {
        rc=prox_ni_exchange(&install_state,thread,action,state.cancel,&message,now);
        pspSdkEnableInterrupts(mask);pspSdkSetK1(k1);return rc;
    }
#endif
    if(message.protocol==PROX_NI_PROTOCOL && message.lease==state.lease && message.generation==state.generation &&
       state.active && state.phase==PROX_NQ_HANDOFF && !state.fault) {
#ifdef PROX_NATIVE_MOUNT_ACCESS
        if((action==PROX_NI_ARM || action==PROX_NI_COMMIT) && (access_busy || prox_native_mount_active())) {
            pspSdkEnableInterrupts(mask);pspSdkSetK1(k1);return -1;
        }
#endif
        if(action==PROX_NI_ARM && thread==state.owner && !state.cancel &&
           memchr(message.job.source_version,0,sizeof(message.job.source_version)) &&
           !strcmp(message.job.source_version,PROX_PRODUCT_VERSION))
#ifdef PROX_NATIVE_DEDICATED_OTA
            rc=prox_native_completion_armed(state.generation) ?
                prox_ni_arm_dedicated(&install_state,thread,state.consumer,state.answer.version,state.answer.package_size,&message,now) : -1;
#else
            rc=prox_ni_arm(&install_state,thread,state.consumer,state.lease,state.generation,state.answer.version,&message.job,now);
#endif
        else if(action!=PROX_NI_ARM && ((action!=PROX_NI_BIND && action!=PROX_NI_CLAIM) || loaded==message.module))
            rc=prox_ni_exchange(&install_state,thread,action,state.cancel,&message,now);
    }
    pspSdkEnableInterrupts(mask);
    if(!rc)memcpy(packet,&message,sizeof(message));
    pspSdkSetK1(k1);return rc;
#else
    (void)action;(void)packet;(void)size;return -1;
#endif
}
#ifdef PROX_NATIVE_MOUNT_ACCESS
typedef struct { int action,result,completed;ProxNativeInstallMessage *packet;unsigned int size; } AccessCall;
#ifdef PROX_NATIVE_DEDICATED_OTA
static int cleanup_service(const void *packet)
{
    ProxNativeCleanupMessage cleanup;
    memcpy(&cleanup,packet,sizeof(cleanup));
    int thread=sceKernelGetThreadId();
    unsigned int mask=pspSdkDisableInterrupts();
    int admitted=cleanup.protocol==PROX_NI_CLEANUP_PROTOCOL && cleanup.lease==state.lease &&
        cleanup.generation==state.generation && thread>0 && thread==state.owner &&
        state.active && state.phase==PROX_NQ_QUERY &&
        (state.command==PROX_NQ_CHECK || state.command==PROX_NQ_BOOT_CLEANUP) && !state.fault && !state.cancel &&
        !access_busy && !prox_ni_locked(&install_state) && !prox_native_mount_active();
    if(admitted)access_busy=1;
    pspSdkEnableInterrupts(mask);
    if(!admitted)return -1;

    int result=prox_native_cleanup_flash(&cleanup);
    mask=pspSdkDisableInterrupts();
    access_busy=0;
    if(result==-2) { state.fault=1;state.answer.failure=PROX_NQ_CAUSE_CLIENT; }
    pspSdkEnableInterrupts(mask);
    return result;
}
#endif
static void access_extended(void *arg)
{
    AccessCall *call=arg;
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(call->action==PROX_NI_CLEANUP_COMMITTED)
        call->result=cleanup_service(call->packet);
    else
#endif
    call->result=install_service_inner(call->action,call->packet,call->size);
    call->completed=1;
}
#endif
int proxNativeInstallService(int action,ProxNativeInstallMessage *packet,unsigned int size)
{
#ifdef PROX_NATIVE_MOUNT_ACCESS
    if(action==PROX_NI_ACCESS_PROBE || action==PROX_NI_ACCESS_BEGIN || action==PROX_NI_ACCESS_END
#ifdef PROX_NATIVE_DEDICATED_OTA
       || action==PROX_NI_CLEANUP_COMMITTED
#endif
      ) {
        AccessCall call;unsigned int mask;int k1,rc,busy;
        unsigned int expected=sizeof(*packet);
#ifdef PROX_NATIVE_DEDICATED_OTA
        if(action==PROX_NI_CLEANUP_COMMITTED)expected=sizeof(ProxNativeCleanupMessage);
#endif
        if(!packet || size!=expected)return -1;
#ifdef PROX_NQ_PROVIDER_TEST
        if(!prox_native_test_valid(packet,size))return -1;
#else
        { unsigned int address=(unsigned int)packet;
          if((address&3u) || address<0x08800000u || address>0x0c000000u-size)return -1; }
#endif
        mask=pspSdkDisableInterrupts();busy=access_busy;pspSdkEnableInterrupts(mask);
        if(busy)return -1;
        call.action=action;call.packet=packet;call.size=size;call.result=-1;call.completed=0;
        k1=pspSdkSetK1(0);
        /* The entry frame stays small. The copied job and deep FAT/export/I/O
         * calls execute on a bounded 16 KiB kernel stack, not the VSH thread's
         * small syscall stack. Extension failure invokes no access callback.
         * Ordinary FAIL remains available without allocating an extra stack. */
        rc=sceKernelExtendKernelStack(0x4000,access_extended,&call);
        pspSdkSetK1(k1);
        return call.completed?call.result:(rc<0?rc:-1);
    }
#endif
    return install_service_inner(action,packet,size);
}
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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    unsigned int now=sceKernelGetSystemTimeLow();
#endif
    if(size!=sizeof(message) || !valid(packet,size)) return -1;
    k1=pspSdkSetK1(0); memcpy(&message,packet,sizeof(message)); thread=sceKernelGetThreadId();
    mask=pspSdkDisableInterrupts();
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    if(action==PROX_NQ_NOTICE_ARM || action==PROX_NQ_NOTICE_STATUS) {
        rc=message.protocol==PROX_NQ_PROTOCOL && message.lease==state.lease &&
           message.generation==state.generation && thread==state.owner && state.active &&
           !state.cancel && !state.fault && state.phase==PROX_NQ_HANDOFF?0:-1;
        if(!rc && action==PROX_NQ_NOTICE_ARM)
            rc=prox_native_completion_arm(state.generation,(const unsigned short *)message.summary,message.downloaded);
        if(!rc && action==PROX_NQ_NOTICE_STATUS) message.result=prox_native_completion_health(state.generation,now);
    } else
#endif
    rc=prox_nq_exchange(&state,thread,action,&message);
    pspSdkEnableInterrupts(mask);
    if(!rc) memcpy(packet,&message,sizeof(message));
    pspSdkSetK1(k1); return rc;
}
void prox_native_query_load_failed(void)
{ prox_native_query_load_failed_reason(PROX_NQ_CAUSE_CLIENT); }
void prox_native_query_load_failed_reason(unsigned int failure)
{
    unsigned int mask=pspSdkDisableInterrupts();
#ifdef PROX_NATIVE_DEDICATED_OTA
    if (state.command==PROX_NQ_BOOT_CLEANUP) {
        state.active=0;
        state.command=PROX_NQ_IDLE;
    }
#endif
    state.fault=1; state.cancel=1; state.done=1; state.answer.result=-1;
    state.answer.failure=failure<=PROX_NQ_CAUSE_CLIENT?failure:PROX_NQ_CAUSE_CLIENT;
    pspSdkEnableInterrupts(mask);
}
#ifdef PROX_NATIVE_DEDICATED_OTA
int prox_native_query_request_boot_cleanup(void)
{
    unsigned int seed=sceKernelGetSystemTimeLow()|1u;
    unsigned int mask=pspSdkDisableInterrupts();
    if (!state.lease)prox_nq_reset(&state,seed);
    int result=prox_nq_start_boot_cleanup(&state);
    unsigned int lease=state.lease;
    pspSdkEnableInterrupts(mask);
    if (result)return result;
    result=prox_native_query_start(lease);
    if (result<0)prox_native_query_load_failed();
    return result;
}
#endif
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
    unsigned int mask,lease,start; int rc,k1,loaded,loading;
    if(!allocate || !release) return NATIVE_ARGUMENT;
#if defined(PROX_NATIVE_MOUNT_ACCESS) && !defined(PROX_NATIVE_DEDICATED_OTA)
    /* Preserve the old owner's restoration token after a failed cleanup. */
    if(prox_native_mount_active())return NATIVE_STATE;
#endif
    k1=pspSdkSetK1(0);
    start=sceKernelGetSystemTimeLow();
#ifdef PROX_NATIVE_DEDICATED_OTA
    /* An early Settings request can share the resident worker after the
     * startup operation has released it. It cannot cancel the cleanup. */
    for (;;) {
        loaded=sceKernelFindModuleByName("ProxUpdater")!=NULL;
        loading=prox_native_query_is_loading();
        mask=pspSdkDisableInterrupts();
        if (state.command!=PROX_NQ_BOOT_CLEANUP)break;
        pspSdkEnableInterrupts(mask);
        if ((unsigned int)(sceKernelGetSystemTimeLow()-start)>10000000u) {
            pspSdkSetK1(k1);
            return NATIVE_STATE;
        }
        sceKernelDelayThread(10000);
    }
    if (prox_native_mount_active()) {
        pspSdkEnableInterrupts(mask);
        pspSdkSetK1(k1);
        return NATIVE_STATE;
    }
#else
    loaded=sceKernelFindModuleByName("ProxUpdater")!=NULL; loading=prox_native_query_is_loading();
    mask=pspSdkDisableInterrupts();
#endif
    if(!state.lease || (!loaded && !loading && !state.active)) prox_nq_reset(&state,start|1u);
    rc=prox_nq_open(&state); lease=state.lease;
    pspSdkEnableInterrupts(mask);
    if(!rc) rc=prox_native_query_start(lease);
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    if(!rc) prox_native_completion_reset();
#ifdef PROX_NATIVE_BACKGROUND
    if(!rc) memset(&install_state,0,sizeof(install_state));
#endif
#endif
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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    if(prox_native_install_preview_locked()) return 0;
#endif
    unsigned int mask=pspSdkDisableInterrupts();
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(state.command==PROX_NQ_BOOT_CLEANUP) {
        pspSdkEnableInterrupts(mask);
        return 0;
    }
#endif
    prox_nq_cancel(&state); pspSdkEnableInterrupts(mask);
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
#if defined(PROX_NATIVE_UI_QUERY) && !defined(PROX_NATIVE_OTA) && !defined(PROX_NATIVE_OFFER_PREVIEW)
    prox_native_query_event("profile worker ready wait us", (int)(sceKernelGetSystemTimeLow() - start));
#endif
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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
#ifdef PROX_NATIVE_DEDICATED_OTA
int prox_native_offer_storage_text(unsigned short *destination)
{
    unsigned int mask=pspSdkDisableInterrupts();
    unsigned int count=state.answer.downloaded;
    int ready=state.done && state.answer.result==1 && state.generation && !state.active &&
        !state.cancel && !state.fault && state.phase==PROX_NQ_QUERY &&
        count>=2 && count<=PROX_OFFER_TEXT_UNITS;
    if(ready) {
        unsigned int last=(count-1)*sizeof(*destination);
        ready=state.answer.summary[0] && !state.answer.summary[last] && !state.answer.summary[last+1];
        if(ready)
            memcpy(destination,state.answer.summary,count*sizeof(*destination));
    }
    pspSdkEnableInterrupts(mask);
    return ready;
}
#endif
unsigned int prox_native_completion_generation(void)
{
    unsigned int mask=pspSdkDisableInterrupts(),generation=0;
    if(state.active && !state.cancel && !state.fault && state.phase==PROX_NQ_HANDOFF) generation=state.generation;
    pspSdkEnableInterrupts(mask);return generation;
}
#endif
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
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
            if(state.phase==PROX_NQ_HANDOFF && prox_native_completion_ready(state.generation)) rc=2;
#endif
        }
        pspSdkEnableInterrupts(mask);
        if(launch) prox_native_query_event("native verified stage; launching target installer",0);
        if(rc<=0 || rc==2) return rc;
        if((unsigned int)(sceKernelGetSystemTimeLow()-transfer_started)>300000000u) {
#ifdef PROX_NATIVE_BACKGROUND
            if(prox_native_install_critical()) { sceKernelDelayThread(10000);continue; }
#endif
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
