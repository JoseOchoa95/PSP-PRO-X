#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PROX_NQ_PROVIDER_TEST 1
#define PROX_NATIVE_BACKGROUND 1
#define PROX_NATIVE_MOUNT_ACCESS 1
#include "../../firmware/Vshctrl/native_query_provider.c"
static int irq,k1=0x1234,thread=4,module=17,called,invalid,extended,allocation_fail,mount_active;
int prox_native_test_valid(const void *p,unsigned int n){return !invalid && p && n==sizeof(ProxNativeInstallMessage);}
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
            assert(!proxNativeInstallService(PROX_NI_ACCESS_BEGIN,&m,sizeof(m)) && !m.result && mount_active && m.access.active);
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
