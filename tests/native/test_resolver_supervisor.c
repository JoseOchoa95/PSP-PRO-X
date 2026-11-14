/* Execute the production PSP supervision branch with scheduled SDK events;
 * TLS-loopback tests deliberately use a different DNS adapter. */
#define PROX_DNS_NATIVE
#include "../../firmware/Updater/resolver.c"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>

enum { IMMEDIATE,DELAYED,SDK_ERROR,WATCHDOG,WAIT_ERROR,CANCEL,REQUEST_DEADLINE,
       STOP_ERROR,STRAGGLER,CREATE_ERROR,THREAD_ERROR,START_ERROR,THREAD_DELETE_ERROR,
       RESOLVER_DELETE_ERROR,EARLY_THREAD_END,LATE_END,POSITIVE_SDK };
static int scenario,thread_live,resolver_live,stop_calls,wait_calls,dns_calls;
static int delete_threads,delete_resolvers,link_reads,fatal_calls;
static unsigned int ticks;
static int (*entry)(SceSize,void *);
static char logs[8192];
static jmp_buf fatal_jump;
void prox_log(const char *format,...)
{ size_t n=strlen(logs); va_list a; va_start(a,format); vsnprintf(logs+n,sizeof(logs)-n,format,a); va_end(a); strcat(logs,"\n"); }
int prox_stopping(void) { return scenario == CANCEL; }
unsigned int sceKernelGetSystemTimeLow(void) { return ticks; }
int sceKernelDelayThread(unsigned int t) { ticks+=t; return 0; }
int sceNetResolverCreate(int *id,void *scratch,SceSize size)
{
    assert(size == 1024 && scratch == dns.scratch);
    if (scenario == CREATE_ERROR) return -2;
    resolver_live=1; *id=77; return 0;
}
int sceNetResolverDelete(int id)
{
    assert(id == 77 && !thread_live); ++delete_resolvers;
    if (scenario == RESOLVER_DELETE_ERROR) return -4;
    resolver_live=0; return 0;
}
int sceKernelCreateThread(const char *name,int (*function)(SceSize,void *),int priority,int stack,unsigned int attrs,void *option)
{
    assert(!strcmp(name,"PRO-X DNS") && priority == 0x18 && stack == 0x4000 && attrs == PSP_THREAD_ATTR_USER && !option);
    entry=function; return scenario == THREAD_ERROR ? -3 : 31;
}
int sceNetResolverStartNtoA(int id,const char *host,struct in_addr *address,unsigned int timeout,int retries)
{
    assert(id == 77 && !strcmp(host,"api.github.com") && timeout == 5 && retries == 1);
    ++dns_calls;
    address->s_addr=0x12345678;
    return scenario == SDK_ERROR ? (int)0x80410410 : scenario == POSITIVE_SDK ? 1 : stop_calls ? -9 : 0;
}
static void complete(void) { assert(thread_live); entry(0,NULL); thread_live=0; }
int sceKernelStartThread(int id,unsigned int size,void *arg)
{
    assert(id == 31 && size == 0 && !arg);
    if (scenario == START_ERROR) return -5;
    thread_live=1;
    if (scenario == IMMEDIATE) complete();
    return 0;
}
int sceKernelWaitThreadEnd(int id,SceUInt *wait)
{
    assert(id == 31 && wait); ++wait_calls;
    if (!thread_live) return 0;
    if (stop_calls) {
        assert(*wait == 1000000);
        if (scenario == STRAGGLER) { ticks+=*wait; return SCE_KERNEL_ERROR_WAIT_TIMEOUT; }
        complete(); return 0;
    }
    assert(*wait == 50000);
    if (scenario == WAIT_ERROR) { ticks+=1000; return (int)0x800201a5; }
    if (scenario == EARLY_THREAD_END) { thread_live=0; return 0; }
    if (scenario == LATE_END) { ticks+=12000000; complete(); return 0; }
    if (scenario == SDK_ERROR || scenario == THREAD_DELETE_ERROR || scenario == RESOLVER_DELETE_ERROR ||
        scenario == POSITIVE_SDK ||
        (scenario == DELAYED && wait_calls == 3)) { complete(); return 0; }
    ticks+=*wait; return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
}
int sceNetResolverStop(int id)
{ assert(id == 77 && thread_live); ++stop_calls; return scenario == STOP_ERROR ? -6 : 0; }
int sceKernelDeleteThread(int id)
{ assert(id == 31 && !thread_live); ++delete_threads; return scenario == THREAD_DELETE_ERROR ? -7 : 0; }
int sceNetApctlGetState(int *state)
{ assert(!thread_live); ++link_reads; *state=4; return 0; }
int sceKernelExitGame(void)
{ assert(thread_live && resolver_live); ++fatal_calls; longjmp(fatal_jump,1); }
int sceKernelExitDeleteThread(int status) { (void)status; assert(0); return 0; }
static void reset(int which)
{
    scenario=which; ticks=0; thread_live=resolver_live=stop_calls=wait_calls=dns_calls=0;
    delete_threads=delete_resolvers=link_reads=fatal_calls=0;
    memset(logs,0,sizeof(logs));
}
int main(void)
{
    struct in_addr address;
    int rc,i;
    for (i=IMMEDIATE;i<=DELAYED;++i) {
        reset(i); assert(!prox_psp_resolve("api.github.com",&address,0));
        assert(address.s_addr == 0x12345678 && dns_calls == 1 && !stop_calls);
        assert(delete_threads == 1 && delete_resolvers == 1 && !resolver_live);
    }
    reset(SDK_ERROR); address.s_addr=0;
    assert(prox_psp_resolve("api.github.com",&address,0)==PROX_CONNECT_RETRYABLE && !address.s_addr && !stop_calls && link_reads == 1);
    assert(strstr(logs,"80410410"));
    reset(WATCHDOG); rc=prox_psp_resolve("api.github.com",&address,0);
    assert(rc==PROX_CONNECT_RETRYABLE && ticks == 12000000 && stop_calls == 1 && delete_threads == 1 && delete_resolvers == 1);
    assert(strstr(logs,"lookup_watchdog=1 unexpected_wait=0"));
    reset(WAIT_ERROR); assert(prox_psp_resolve("api.github.com",&address,0)==-1);
    assert(ticks == 1000 && wait_calls == 2 && stop_calls == 1 && strstr(logs,"unexpected_wait=1"));
    reset(CANCEL); assert(prox_psp_resolve("api.github.com",&address,0)<0 && stop_calls == 1);
    assert(strstr(logs,"cancelled=1 request_deadline=0 lookup_watchdog=0"));
    reset(REQUEST_DEADLINE); assert(prox_psp_resolve("api.github.com",&address,0u-90000001u)<0);
    assert(strstr(logs,"request_deadline=1 lookup_watchdog=0"));
    reset(STOP_ERROR); assert(prox_psp_resolve("api.github.com",&address,0)==-1 && delete_resolvers == 1);
    assert(strstr(logs,"DNS stop rc=0xfffffffa join=0x00000000"));
    reset(STRAGGLER);
    if (!setjmp(fatal_jump)) { prox_psp_resolve("api.github.com",&address,0); assert(0); }
    assert(fatal_calls == 1 && !delete_threads && !delete_resolvers && thread_live && resolver_live);
    reset(CREATE_ERROR); assert(prox_psp_resolve("api.github.com",&address,0)<0 && !delete_resolvers);
    reset(THREAD_ERROR); assert(prox_psp_resolve("api.github.com",&address,0)<0 && delete_resolvers == 1 && !delete_threads);
    reset(START_ERROR); assert(prox_psp_resolve("api.github.com",&address,0)<0 && delete_resolvers == 1 && delete_threads == 1);
    reset(THREAD_DELETE_ERROR); assert(prox_psp_resolve("api.github.com",&address,0)<0 && link_reads == 1);
    reset(RESOLVER_DELETE_ERROR); assert(prox_psp_resolve("api.github.com",&address,0)<0 && link_reads == 1);
    reset(EARLY_THREAD_END); assert(prox_psp_resolve("api.github.com",&address,0)<0 && !dns_calls);
    reset(LATE_END); address.s_addr=0;
    assert(prox_psp_resolve("api.github.com",&address,0)<0 && !address.s_addr && !stop_calls && strstr(logs,"completion rejected"));
    reset(POSITIVE_SDK); address.s_addr=0;
    assert(prox_psp_resolve("api.github.com",&address,0)<0 && !address.s_addr && dns_calls == 1);
    reset(WATCHDOG); ticks=0xffff0000u;
    { unsigned int start=ticks; assert(prox_psp_resolve("api.github.com",&address,start)<0 && (unsigned int)(ticks-start) == 12000000); }
    puts("PASS: production PSP DNS supervisor, exact wait-error handling, cancellation/deadlines, worker ownership, cleanup, fatal straggler and clock wrap");
    return 0;
}
