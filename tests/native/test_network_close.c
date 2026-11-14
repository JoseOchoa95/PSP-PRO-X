#define PROX_NETWORK_NATIVE
#include "../../firmware/Updater/network_close.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
enum { NORMAL,ALREADY_DOWN,RESOLVER_FAIL,QUERY_FAIL,DISCONNECT_FAIL,STUCK,
       POLL_FAIL,APCTL_FAIL,INET_FAIL,NET_FAIL,INET_MODULE_FAIL,COMMON_FAIL };
static int scenario,state,disconnects,polls,apctl_terms,inet_terms,net_terms,unloads;
static unsigned int ticks;
static char order[32],logs[4096];
static void append(char c) { size_t n=strlen(order); order[n]=c; order[n+1]=0; }
void prox_log(const char *format,...)
{ size_t n=strlen(logs); va_list a; va_start(a,format); vsnprintf(logs+n,sizeof(logs)-n,format,a); va_end(a); strcat(logs,"\n"); }
unsigned int sceKernelGetSystemTimeLow(void) { return ticks; }
int sceKernelDelayThread(unsigned int n) { ticks+=n; return 0; }
int sceNetResolverTerm(void) { append('R'); return scenario == RESOLVER_FAIL ? -1 : 0; }
int sceNetApctlGetState(int *out)
{
    if (scenario == QUERY_FAIL || (scenario == POLL_FAIL && disconnects)) return -2;
    if (disconnects && scenario != STUCK && ++polls == 3) state=0;
    *out=state; return 0;
}
int sceNetApctlDisconnect(void) { ++disconnects; return scenario == DISCONNECT_FAIL ? -3 : 0; }
int sceNetApctlTerm(void) { assert(state == 0); ++apctl_terms; append('A'); return scenario == APCTL_FAIL ? -4 : 0; }
int sceNetInetTerm(void) { ++inet_terms; append('I'); return scenario == INET_FAIL ? -5 : 0; }
int sceNetTerm(void) { ++net_terms; append('N'); return scenario == NET_FAIL ? -6 : 0; }
int sceUtilityUnloadNetModule(int module)
{
    ++unloads;
    if (module == PSP_NET_MODULE_INET) { append('E'); return scenario == INET_MODULE_FAIL ? -7 : 0; }
    assert(module == PSP_NET_MODULE_COMMON); append('C'); return scenario == COMMON_FAIL ? -8 : 0;
}
static void reset(int value)
{
    scenario=value; state=value == ALREADY_DOWN ? 0 : 4; ticks=0;
    disconnects=polls=apctl_terms=inet_terms=net_terms=unloads=0; memset(order,0,sizeof(order)); memset(logs,0,sizeof(logs));
}
int main(void)
{
    int i;
    reset(NORMAL); assert(!prox_network_close(1,1,1,1,1,1));
    assert(!strcmp(order,"RAINEC") && disconnects == 1 && polls == 3 && ticks == 40000);
    reset(ALREADY_DOWN); assert(!prox_network_close(1,1,1,1,1,1) && !disconnects && !ticks);
    reset(RESOLVER_FAIL); assert(prox_network_close(1,1,1,1,1,1)<0 && !strcmp(order,"R"));
    for (i=QUERY_FAIL;i<=POLL_FAIL;++i) {
        reset(i); assert(prox_network_close(1,1,1,1,1,1)<0);
        assert(!apctl_terms && !inet_terms && !net_terms && !unloads && !strcmp(order,"R"));
        assert(strstr(logs,"installer handoff blocked"));
        if (i == STUCK) assert(ticks == 5000000);
    }
    reset(APCTL_FAIL); assert(prox_network_close(1,1,1,1,1,1)<0 && !inet_terms && !unloads);
    reset(INET_FAIL); assert(prox_network_close(1,1,1,1,1,1)<0 && !net_terms && !unloads);
    reset(NET_FAIL); assert(prox_network_close(1,1,1,1,1,1)<0 && !unloads);
    reset(INET_MODULE_FAIL); assert(prox_network_close(1,1,1,1,1,1)<0 && unloads == 1);
    reset(COMMON_FAIL); assert(prox_network_close(1,1,1,1,1,1)<0 && unloads == 2);
    reset(NORMAL); assert(!prox_network_close(0,0,0,1,1,1) && !strcmp(order,"NEC") && !disconnects);
    reset(NORMAL); assert(!prox_network_close(0,0,0,0,0,0) && !order[0]);
    reset(STUCK); ticks=0xffff0000u;
    { unsigned int start=ticks; assert(prox_network_close(1,1,1,1,1,1)<0 && (unsigned int)(ticks-start) == 5000000); }
    puts("PASS: production network teardown, asynchronous disconnect, checked term/unload failures, dependency lifetime, partial init and clock wrap");
    return 0;
}
