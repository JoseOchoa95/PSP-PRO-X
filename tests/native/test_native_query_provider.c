#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_query_provider.h"
#include "prox_native_query.h"
static int thread=1,locked,k1=55,loaded,reply_mode,reply_enabled=1;
static unsigned int ticks,lease_id;
static int current_events,error_events;
#ifdef PROX_NATIVE_OFFER_PREVIEW
static int read_result=-1,read_calls;
void prox_native_preview_reset(void) { }
void prox_native_preview_abort(void) { }
void prox_native_preview_offer(unsigned int size) { assert(size==1024u*1024u); }
int prox_native_preview_read(void *buffer,unsigned int capacity)
{ (void)buffer; (void)capacity; ++read_calls; return read_result; }
#endif
int prox_native_test_valid(const void *p,unsigned int n) { return p && n && n<=sizeof(ProxNativeQueryMessage); }
int pspSdkSetK1(int n) { int old=k1; k1=n; return old; }
unsigned int pspSdkDisableInterrupts(void) { assert(!locked); locked=1; return 0x12; }
void pspSdkEnableInterrupts(unsigned int n) { assert(locked && n==0x12); locked=0; }
int sceKernelGetThreadId(void) { assert(!locked); return thread; }
unsigned int sceKernelGetSystemTimeLow(void) { assert(!locked); return ticks; }
void *sceKernelFindModuleByName(const char *s) { (void)s; assert(!locked); return loaded?(void *)1:0; }
int prox_native_query_is_loading(void) { return 0; }
void prox_native_query_event(const char *s,int rc)
{
    (void)s; assert(!locked);
    if(rc==(int)0x805f0004) ++current_events;
    if(rc==(int)0x805f0007) ++error_events;
}
int prox_native_query_start(unsigned int lease)
{
    ProxNativeQueryMessage m={0}; assert(!locked);
    if(loaded) return 0;
    loaded=1; lease_id=lease; m.protocol=PROX_NQ_PROTOCOL; m.lease=lease; thread=2;
    assert(!proxNativeQueryService(PROX_NQ_REGISTER,&m,sizeof(m))); thread=1; return 0;
}
int sceKernelDelayThread(unsigned int us)
{
    ProxNativeQueryMessage m={0}; assert(!locked); ticks+=us;
    m.protocol=PROX_NQ_PROTOCOL; m.lease=lease_id; thread=2;
    assert(!proxNativeQueryService(PROX_NQ_POLL,&m,sizeof(m)));
    if(m.command==PROX_NQ_CHECK && reply_enabled) {
        m.result=reply_mode; assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m)));
    }
    thread=1; return 0;
}
int main(void)
{
    int (*init)(const void *,const void *)=prox_native_query_function(0xf6690a9a);
    int (*connect)(unsigned int *,unsigned int *)=prox_native_query_function(0xfd675e8d);
    int (*end)(void)=prox_native_query_function(0xd6a09757);
    unsigned int size=99,detail=99; ProxNativeQueryMessage m={0};
    int expected_current=1;
#ifdef PROX_NATIVE_OFFER_PREVIEW
    expected_current=0;
#endif
    assert(init(0,(void *)1)==(int)0x805f000b);
    assert(!init((void *)1,(void *)2) && k1==55);
    assert(connect(0,&detail)==(int)0x805f000b);
#ifdef PROX_NATIVE_OFFER_PREVIEW
    assert(!connect(&size,&detail) && size==1024u*1024u && !detail && !current_events);
#else
    assert(connect(&size,&detail)==(int)0x805f0004 && !size && !detail && current_events==1);
#endif
    assert(!end() && !init((void *)1,(void *)2));
#ifdef PROX_NATIVE_PROGRESS_PREVIEW
    {
        int (*read)(void *,unsigned int,unsigned int *)=prox_native_query_function(0xfa9aa797);
        unsigned char bytes[32]; int before=read_calls;
        assert(read(bytes,sizeof(bytes),0)==(int)0x805f000b && read_calls==before);
        read_result=(int)0x80010013;
        assert(read(bytes,sizeof(bytes),&detail)==(int)0x805f0007 && !detail);
        read_result=-1; assert(read(bytes,sizeof(bytes),&detail)==(int)0x805f0007);
        read_result=0; assert(read(bytes,sizeof(bytes),&detail)==(int)0x805f0007);
        read_result=33; assert(read(bytes,sizeof(bytes),&detail)==(int)0x805f0007);
        read_result=16; assert(read(bytes,sizeof(bytes),&detail)==16 && !detail);
    }
#endif
    reply_mode=-1; assert(connect(&size,&detail)==(int)0x805f0007 && current_events==expected_current);
    assert(!end() && !init((void *)1,(void *)2));
    reply_mode=1; assert(connect(&size,&detail)==(int)0x805f0007 && current_events==expected_current);
    assert(!end() && !init((void *)1,(void *)2));
    reply_enabled=0; assert(connect(&size,&detail)==(int)0x805f0007 && ticks>=120000000u);
    m.protocol=PROX_NQ_PROTOCOL; m.lease=lease_id; thread=2;
    assert(!proxNativeQueryService(PROX_NQ_POLL,&m,sizeof(m)) && m.cancel);
    m.result=0; assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))); thread=1;
    assert(current_events==expected_current && error_events>=3 && !locked && k1==55);
    puts("PASS: actual native provider current only after worker success, errors/new-target/deadline never current, bounded RPC and interrupt restoration");
    return 0;
}
