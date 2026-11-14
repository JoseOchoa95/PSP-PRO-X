/* Execute the actual close/reinit/reconnect ownership path, not a no-op reset. */
#define PROX_NETWORK_NATIVE
#define PROX_SESSION_NATIVE
#include "../../firmware/Updater/network_close.c"
#include "../../firmware/Updater/network_session.c"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int fail,step,stopping,up,draining,stuck,connected,last_profile,no_ip;
static unsigned int stop_at;
static int live_net,live_inet,live_apctl,live_resolver,modules,terminations[4];
static unsigned int ticks;
void prox_log(const char *f,...) { (void)f; }
int prox_stopping(void) { return stopping || (stop_at && ticks >= stop_at); }
unsigned int sceKernelGetSystemTimeLow(void) { return ticks; }
int sceKernelDelayThread(unsigned int us) { ticks+=us; return 0; }
static int result(void) { return ++step == fail ? -1 : 0; }
int sceUtilityLoadNetModule(int m) { (void)m; if (result()) return -1; ++modules; return 0; }
int sceUtilityUnloadNetModule(int m) { (void)m; if (result()) return -1; --modules; return 0; }
int sceUtilityCheckNetParam(int p) { return p == 7 ? 0 : -1; }
int sceNetInit(int a,int b,int c,int d,int e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; assert(!live_net); if (result()) return -1; live_net=1; return 0; }
int sceNetInetInit(void) { assert(live_net && !live_inet); if (result()) return -1; live_inet=1; return 0; }
int sceNetApctlInit(int a,int b) { (void)a;(void)b; assert(live_inet && !live_apctl); if (result()) return -1; live_apctl=1; up=0; return 0; }
int sceNetResolverInit(void) { assert(live_apctl && !live_resolver); if (result()) return -1; live_resolver=1; return 0; }
int sceNetResolverTerm(void) { assert(live_resolver); if (result()) return -1; live_resolver=0; ++terminations[0]; return 0; }
int sceNetApctlTerm(void) { assert(!up && live_apctl && !live_resolver); if (result()) return -1; live_apctl=0; ++terminations[1]; return 0; }
int sceNetInetTerm(void) { assert(live_inet && !live_apctl); if (result()) return -1; live_inet=0; ++terminations[2]; return 0; }
int sceNetTerm(void) { assert(live_net && !live_inet); if (result()) return -1; live_net=0; ++terminations[3]; return 0; }
int sceNetApctlConnect(int p) { assert(live_resolver && !up && !draining); if (result()) return -1; ++connected; last_profile=p; up=1; return 0; }
int sceNetApctlDisconnect(void) { if (result()) return -1; draining=2; return 0; }
int sceNetApctlGetState(int *s)
{
    if (result()) return -1;
    if (draining && !stuck && !--draining) up=0;
    *s=up ? (no_ip ? 1 : 4) : 0; return 0;
}
static void reset(void)
{
    memset(&network,0,sizeof(network)); saved_profile=-1;
    fail=step=stopping=up=draining=stuck=connected=last_profile=no_ip=0; stop_at=0;
    live_net=live_inet=live_apctl=live_resolver=modules=0; ticks=0;
    memset(terminations,0,sizeof(terminations));
}
int main(void)
{
    int i,base;
    reset(); assert(!prox_network_begin() && !prox_network_connect(7)); base=step;
    assert(!prox_network_retry(0));
    assert(connected == 2 && last_profile == 7 && modules == 2);
    for (i=0;i<4;++i) assert(terminations[i] == 1);
    assert(!prox_network_end() && !modules && !live_net);
    for (i=1;i<=6;++i) { /* partial initializations leave accurate close ownership */
        reset(); fail=i; assert(prox_network_begin()<0); fail=0;
        assert(!prox_network_end() && !modules && !live_net);
    }
    /* Every close/reinit/connect failure stops retry; cleanup resumes only
     * for resources still owned, never double-terminates already closed ones. */
    for (i=1;i<=14;++i) {
        reset(); assert(!prox_network_begin() && !prox_network_connect(7));
        fail=base+i;
        assert(prox_network_retry(0)<0); fail=0;
        assert(!prox_network_end() && !modules && !live_net);
    }
    reset(); assert(!prox_network_begin() && !prox_network_connect(7)); stopping=1;
    assert(prox_network_retry(0)<0 && connected == 1); stopping=0; assert(!prox_network_end());
    reset(); assert(!prox_network_begin() && !prox_network_connect(7)); ticks=90000001u;
    assert(prox_network_retry(0)<0 && connected == 1); assert(!prox_network_end());
    reset(); assert(!prox_network_begin() && !prox_network_connect(7)); stuck=1;
    assert(prox_network_retry(0)<0 && ticks == 5000000 && connected == 1); stuck=0; assert(!prox_network_end());
    reset(); assert(!prox_network_begin()); assert(prox_network_connect(99)<0 && !connected); assert(!prox_network_end());
    reset(); assert(!prox_network_begin() && !prox_network_connect(7)); no_ip=1; stop_at=60000;
    assert(prox_network_retry(0)<0 && ticks == 60000 && connected == 2); stop_at=0; assert(!prox_network_end());
    reset(); assert(!prox_network_begin() && !prox_network_connect(7)); no_ip=1; ticks=89900000u;
    assert(prox_network_retry(0)<0 && ticks > 90000000u && ticks < 90100000u && connected == 2);
    assert(!prox_network_end());
    reset(); assert(!prox_network_begin() && !prox_network_connect(7)); ticks=0xfffff000u;
    { unsigned int start=ticks; assert(!prox_network_retry(start) && ticks-start == 20000u); }
    assert(!prox_network_end());
    puts("PASS: production fresh network session, same profile, full teardown/reinit, failure ownership, cancellation and deadline");
    return 0;
}
