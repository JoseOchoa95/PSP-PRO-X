#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PROX_NQ_PROVIDER_TEST 1
#define PROX_NATIVE_BACKGROUND 1
#define PROX_NATIVE_MOUNT_ACCESS 1
#include "../../firmware/Vshctrl/native_query_provider.c"
static int irq,k1=0x1234,thread=4,module=17,called,invalid,extended,allocation_fail,mount_active;
static int capacity_result,capacity_cancel,capacity_calls;
int prox_native_flash_preflight(const ProxNativePart *parts)
{
    assert(parts==install_state.job.parts && !irq && !k1 && access_busy && extended && !mount_active);
    ++capacity_calls;
    if(capacity_cancel)state.cancel=1;
    return capacity_result;
}
#ifdef PROX_NATIVE_DEDICATED_OTA
static int cleanup_result, loader_calls, loader_failure, startup_waits;
static const int resident_module;
void prox_native_query_event(const char *name,int result) { (void)name;(void)result; }
void prox_native_completion_reset(void) { assert(!irq && !k1); }
int prox_native_query_start(unsigned int lease)
{
    assert(!irq && lease==state.lease);
    ++loader_calls;
    return loader_failure;
}
int prox_native_query_is_loading(void) { return 0; }
void *sceKernelFindModuleByName(const char *name)
{
    assert(!irq && !strcmp(name,"ProxUpdater"));
    return (void *)&resident_module;
}
int sceKernelDelayThread(unsigned int delay)
{
    assert(!irq && !k1 && delay==10000 && state.command==PROX_NQ_BOOT_CLEANUP && !startup_waits);
    ProxNativeQueryMessage message={.protocol=PROX_NQ_PROTOCOL,.lease=state.lease,.generation=state.generation};
    assert(!prox_nq_exchange(&state,4,PROX_NQ_RESULT,&message));
    ++startup_waits;
    return 0;
}
int prox_native_completion_armed(unsigned int generation) { assert(generation==45);return 0; }
int prox_native_flash_preflight_dedicated(const ProxNativePart *parts,const ProxNativePart *worker)
{
    assert(worker==&install_state.resident);
    return prox_native_flash_preflight(parts);
}
int prox_native_cleanup_flash(const ProxNativeCleanupMessage *m)
{
    assert(!irq && !k1 && extended && access_busy && thread==state.owner);
    assert(m->lease==31 && m->generation==45);
    ++called;
    assert(proxNativeInstallService(PROX_NI_CLEANUP_COMMITTED,(void *)m,sizeof(*m))<0);
    return cleanup_result;
}
#endif
int prox_native_test_valid(const void *p,unsigned int n)
{
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(n==sizeof(ProxNativeCleanupMessage))return !invalid && p;
#endif
    return !invalid && p && n==sizeof(ProxNativeInstallMessage);
}
int prox_native_install_test_module(void){return module;}
int pspSdkSetK1(int v){int old=k1;k1=v;return old;}
unsigned int pspSdkDisableInterrupts(void){assert(!irq);irq=1;return 7;}
void pspSdkEnableInterrupts(unsigned int v){assert(irq && v==7);irq=0;}
int sceKernelGetThreadId(void){assert(!irq);return thread;}
unsigned int sceKernelGetSystemTimeLow(void){assert(!irq);return 99;}
int sceKernelExtendKernelStack(int size,void (*cb)(void *),void *arg)
{
    assert(size==0x4000 && !irq && !k1 && !extended);
    if(allocation_fail)return -77;
    extended=1;cb(arg);extended=0;return 0;
}
int prox_native_mount_access_probe(ProxMountReport *r)
{
    ProxNativeInstallMessage nested={0};assert(!irq && !k1 && access_busy && extended);++called;
    nested.protocol=1;nested.lease=31;nested.generation=45;nested.module=17;
    assert(proxNativeInstallService(PROX_NI_ACCESS_PROBE,&nested,sizeof(nested))<0);
    memset(r,0,sizeof(*r));r->restored=1;r->stage=9;return -123;
}
int prox_native_mount_active(void){return mount_active;}
int prox_native_mount_begin(ProxMountReport *r)
{assert(extended && !irq && !k1 && !mount_active);mount_active=1;memset(r,0,sizeof(*r));r->active=1;return 0;}
int prox_native_mount_end(ProxMountReport *r)
{assert(extended && !irq && !k1 && mount_active);mount_active=0;memset(r,0,sizeof(*r));r->restored=1;return 0;}
#ifdef PROX_NATIVE_DEDICATED_OTA
int main(void)
{
    for(int case_id=0;case_id<=14;++case_id) {
        ProxNativeCleanupMessage m={.protocol=PROX_NI_CLEANUP_PROTOCOL,.lease=31,.generation=45};
        unsigned int size=sizeof(m);
        memset(&state,0,sizeof(state));
        memset(&install_state,0,sizeof(install_state));
        state.lease=31;state.generation=45;state.active=1;state.phase=PROX_NQ_QUERY;
        state.command=PROX_NQ_CHECK;state.owner=4;
        thread=4;invalid=called=mount_active=access_busy=allocation_fail=cleanup_result=0;
        switch(case_id) {
        case 1:thread=5;break;
        case 2:--m.generation;break;
        case 3:--m.lease;break;
        case 4:--size;break;
        case 5:state.cancel=1;break;
        case 6:state.phase=PROX_NQ_HANDOFF;break;
        case 7:mount_active=1;break;
        case 8:install_state.phase=PROX_NI_WRITING;break;
        case 9:cleanup_result=-2;break;
        case 10:invalid=1;break;
        case 11:allocation_fail=1;break;
        case 12:++m.protocol;break;
        case 13:state.command=PROX_NQ_BOOT_CLEANUP;break;
        case 14:state.command=PROX_NQ_IDLE;break;
        }
        int result=proxNativeInstallService(PROX_NI_CLEANUP_COMMITTED,(void *)&m,size);
        assert(called==(case_id==0 || case_id==9 || case_id==13));
        assert(case_id==0 || case_id==13 ? !result : result<0);
        assert(state.fault==(case_id==9) && !access_busy && !extended && !irq && k1==0x1234);
    }
    puts("PASS: production cleanup RPC requires the current query owner and generation, rejects an active writer, and faults on flash restore failure");
    memset(&state,0,sizeof(state));
    assert(!prox_native_query_request_boot_cleanup());
    assert(loader_calls==1 && state.active && state.command==PROX_NQ_BOOT_CLEANUP);
    assert(prox_native_query_request_boot_cleanup()<0 && loader_calls==1);
    assert(!proxNativeQueryAbort() && !state.cancel);
    ProxNativeQueryMessage message={.protocol=PROX_NQ_PROTOCOL,.lease=state.lease};
    assert(!prox_nq_exchange(&state,4,PROX_NQ_REGISTER,&message));
    assert(!proxNativeQueryInitEx(&message,&message) && startup_waits==1 && loader_calls==2);
    assert(state.ready && !state.active && !state.cancel && !state.done && state.generation==2 && k1==0x1234);
    assert(prox_native_query_request_boot_cleanup()<0 && loader_calls==2);
    memset(&state,0,sizeof(state));
    loader_failure=-1;
    assert(prox_native_query_request_boot_cleanup()<0 && loader_calls==3);
    assert(state.fault && !state.active && state.command==PROX_NQ_IDLE);
    puts("PASS: startup request cannot replace a Settings session; early InitEx waits without cancelling cleanup; loader failure releases its reservation");
}
#else
int main(void)
{
    ProxNativeInstallMessage m;int n;
    for(n=0;n<19;++n){
        memset(&state,0,sizeof(state));memset(&install_state,0,sizeof(install_state));memset(&m,0,sizeof(m));
        state.lease=31;state.generation=45;state.active=1;state.phase=PROX_NQ_HANDOFF;
        install_state.lease=31;install_state.generation=45;install_state.module=17;install_state.engine=4;install_state.phase=PROX_NI_WRITING;
        m.protocol=1;m.lease=31;m.generation=45;m.module=17;thread=4;module=17;called=invalid=access_busy=allocation_fail=extended=mount_active=0;
        switch(n){
        case 1:m.protocol=7;break;case 2:m.lease=30;break;case 3:m.generation=44;break;case 4:state.active=0;break;
        case 5:state.cancel=1;break;case 6:state.fault=1;break;case 7:state.phase=PROX_NQ_VERIFIED;break;
        case 8:thread=1;break;case 9:install_state.phase=PROX_NI_CLAIMED;break;case 10:m.module=18;break;
        case 11:module=-1;break;case 12:install_state.module=19;break;case 13:install_state.lease=99;break;
        case 14:install_state.generation=99;break;case 15:access_busy=1;break;case 16:invalid=1;break;
        case 17:thread=0;install_state.engine=0;break;case 18:allocation_fail=1;break;}
        if(!n){
            install_state.phase=PROX_NI_CLAIMED;
            assert(!proxNativeInstallService(PROX_NI_ACCESS_CAPS,&m,sizeof(m)) && m.result==PROX_NI_ACCESS_REVISION && !called);
            thread=1;assert(proxNativeInstallService(PROX_NI_ACCESS_CAPS,&m,sizeof(m))<0 && !called);thread=4;
            install_state.phase=PROX_NI_WRITING;
            assert(proxNativeInstallService(PROX_NI_ACCESS_CAPS,&m,sizeof(m))<0 && !called);
            assert(!proxNativeInstallService(PROX_NI_ACCESS_PROBE,&m,sizeof(m)));assert(called==1 && m.result==-123 && m.access.restored && m.access.stage==9);
            capacity_result=-4;
            assert(!proxNativeInstallService(PROX_NI_ACCESS_BEGIN,&m,sizeof(m)) && m.result==-4 && !mount_active);
            capacity_result=0;capacity_cancel=1;
            assert(!proxNativeInstallService(PROX_NI_ACCESS_BEGIN,&m,sizeof(m)) && m.result<0 && !mount_active);
            state.cancel=0;capacity_cancel=0;
            assert(!proxNativeInstallService(PROX_NI_ACCESS_BEGIN,&m,sizeof(m)) && !m.result && mount_active && m.access.active);
            assert(capacity_calls==3 && install_state.flash_preflight_passed);
            assert(!proxNativeInstallService(PROX_NI_ACCESS_END,&m,sizeof(m)) && !m.result && !mount_active);
            assert(!proxNativeInstallService(PROX_NI_ACCESS_BEGIN,&m,sizeof(m)) && !m.result && mount_active);
            assert(capacity_calls==3);
            assert(proxNativeInstallService(PROX_NI_COMMIT,&m,sizeof(m))<0 && install_state.phase==PROX_NI_WRITING);
            state.cancel=state.fault=1;state.active=0;state.phase=PROX_NQ_IDLE;
            assert(!proxNativeInstallService(PROX_NI_ACCESS_END,&m,sizeof(m)) && !m.result && !mount_active && m.access.restored);
            assert(!proxNativeInstallService(PROX_NI_FAIL,&m,sizeof(m)) && install_state.phase==PROX_NI_FAILED);
        }
        else {assert(proxNativeInstallService(PROX_NI_ACCESS_PROBE,&m,sizeof(m))<0 && !called);}
        assert(!irq && !extended && k1==0x1234);
        if(n==18){allocation_fail=1;thread=4;assert(!proxNativeInstallService(PROX_NI_FAIL,&m,sizeof(m)));assert(install_state.phase==PROX_NI_FAILED && !extended);}
    }
    assert(proxNativeInstallService(PROX_NI_ACCESS_PROBE,&m,sizeof(m)-1)<0 && proxNativeInstallService(PROX_NI_ACCESS_PROBE,NULL,sizeof(m))<0);
    puts("PASS: actual kernel RPC rejects wrong role/phase/module/nonce/generation/cancel/fault/pointer/size/reentry; 16 KiB extended stack, allocation failure invokes no callback and FAIL still works, IRQ/K1 restored, copied failure report");
}
#endif
