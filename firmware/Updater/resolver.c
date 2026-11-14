/* Checked worker lifecycle; no alternate DNS or address cache.
 * Thread wait failures are not evidence of a DNS-server timeout.
 * GPL-3.0-or-later. */
#ifdef PROX_DNS_NATIVE
#include "resolver_shim.h"
#else
#include <pspkernel.h>
#include <pspkerror.h>
#include <pspnet_resolver.h>
#include <pspnet_apctl.h>
#endif
#include <netinet/in.h>
#include <string.h>
#include "resolver.h"
#include "transport.h"
#include "connect_retry.h"

static struct {
    int resolver,result;
    volatile int entered,finished;
    const char *host;
    struct in_addr address;
    char scratch[1024];
} dns;

static int resolver_worker(SceSize size,void *arg)
{
    (void)size; (void)arg; dns.entered=1;
    dns.result=sceNetResolverStartNtoA(dns.resolver,dns.host,&dns.address,5,1);
    dns.finished=1; return 0;
}
static void link_state(void)
{
    int state=-1,rc=sceNetApctlGetState(&state);
    prox_log("DNS link observation: APCTL rc=0x%08x state=%d",rc,state);
}
static int release(int thread)
{
    int a=sceKernelDeleteThread(thread),b=sceNetResolverDelete(dns.resolver);
    prox_log("DNS release: thread=0x%08x resolver=0x%08x",a,b);
    return a < 0 || b < 0 ? -1 : 0;
}
static void live_worker_exit(void)
{
    prox_log("DNS worker still live; exit to XMB without freeing/unloading its resources");
    sceKernelExitGame(); sceKernelExitDeleteThread(0);
    /* Do not fall through to freeing a live worker's static storage even if an
     * exit syscall unexpectedly returns. Normal PSP exit calls do not return. */
    for (;;) sceKernelDelayThread(1000000);
}
int prox_psp_resolve(const char *host,struct in_addr *address,unsigned int request_start)
{
    unsigned int start=sceKernelGetSystemTimeLow(),now;
    int thread,rc,cancelled,deadline,watchdog;
    SceUInt wait;
    memset(&dns,0,sizeof(dns)); dns.host=host; dns.result=-1;
    rc=sceNetResolverCreate(&dns.resolver,dns.scratch,sizeof(dns.scratch));
    if (rc < 0) { prox_log("DNS resolver create failed rc=0x%08x",rc); return -1; }
    thread=sceKernelCreateThread("PRO-X DNS",resolver_worker,0x18,0x4000,PSP_THREAD_ATTR_USER,NULL);
    if (thread < 0) {
        prox_log("DNS thread create failed rc=0x%08x; delete=0x%08x",thread,sceNetResolverDelete(dns.resolver)); return -1;
    }
    rc=sceKernelStartThread(thread,0,NULL);
    if (rc < 0) { prox_log("DNS thread start failed rc=0x%08x",rc); release(thread); return -1; }
    for (;;) {
        wait=50000; rc=sceKernelWaitThreadEnd(thread,&wait);
        if (!rc) break;
        now=sceKernelGetSystemTimeLow(); cancelled=prox_stopping();
        deadline=(unsigned int)(now-request_start) > 90000000u;
        watchdog=(unsigned int)(now-start) >= 12000000u;
        if (rc != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT || cancelled || deadline || watchdog) {
            int stop,unexpected=rc != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT;
            prox_log("DNS supervisor: wait=0x%08x elapsed_ms=%u entered=%d finished=%d",rc,
                     (unsigned int)(now-start)/1000,dns.entered,dns.finished);
            prox_log("DNS stop reason: cancelled=%d request_deadline=%d lookup_watchdog=%d unexpected_wait=%d",
                     cancelled,deadline,watchdog,rc != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT);
            stop=sceNetResolverStop(dns.resolver); wait=1000000;
            rc=sceKernelWaitThreadEnd(thread,&wait);
            prox_log("DNS stop rc=0x%08x join=0x%08x worker_after_stop=0x%08x finished=%d",stop,rc,dns.result,dns.finished);
            if (rc < 0) live_worker_exit();
            rc=release(thread); link_state();
            return !rc && stop >= 0 && !cancelled && !deadline && !unexpected && watchdog ? PROX_CONNECT_RETRYABLE : -1;
        }
    }
    now=sceKernelGetSystemTimeLow(); cancelled=prox_stopping();
    deadline=(unsigned int)(now-request_start) > 90000000u;
    watchdog=(unsigned int)(now-start) >= 12000000u;
    prox_log("DNS result=0x%08x elapsed_ms=%u entered=%d finished=%d",dns.result,
             (unsigned int)(now-start)/1000,dns.entered,dns.finished);
    if (cancelled || deadline || watchdog)
        prox_log("DNS completion rejected: cancelled=%d request_deadline=%d lookup_watchdog=%d",cancelled,deadline,watchdog);
    rc=release(thread);
    if (rc < 0 || dns.result != 0 || !dns.finished || cancelled || deadline || watchdog) {
        link_state();
        return !rc && dns.result < 0 && dns.finished && !cancelled && !deadline && !watchdog ? PROX_CONNECT_RETRYABLE : -1;
    }
    *address=dns.address; return 0;
}
