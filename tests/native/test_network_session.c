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
static int profile_error;
static int reconnect_no_ip;
static unsigned int profile_change_at;
void prox_log(const char *f,...) { (void)f; }
int prox_stopping(void) { return stopping || (stop_at && ticks >= stop_at); }
unsigned int sceKernelGetSystemTimeLow(void) { return ticks; }
int sceKernelDelayThread(unsigned int us) { ticks+=us; return 0; }
static int result(void) { return ++step == fail ? -1 : 0; }
int sceUtilityLoadNetModule(int m) { (void)m; if (result()) return -1; ++modules; return 0; }
int sceUtilityUnloadNetModule(int m) { (void)m; if (result()) return -1; --modules; return 0; }
int sceUtilityCheckNetParam(int p) { return p == 7 ? 0 : -1; }
int proxUtilityGetLatestProfile(int *p)
{ *p=(profile_change_at && ticks>=profile_change_at)?8:7; return profile_error?-1:0; }
int sceNetInit(int a,int b,int c,int d,int e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; assert(!live_net); if (result()) return -1; live_net=1; return 0; }
int sceNetInetInit(void) { assert(live_net && !live_inet); if (result()) return -1; live_inet=1; return 0; }
int sceNetApctlInit(int a,int b) { (void)a;(void)b; assert(live_inet && !live_apctl); if (result()) return -1; live_apctl=1; up=0; return 0; }
int sceNetResolverInit(void) { assert(live_apctl && !live_resolver); if (result()) return -1; live_resolver=1; return 0; }
int sceNetResolverTerm(void) { assert(live_resolver); if (result()) return -1; live_resolver=0; ++terminations[0]; return 0; }
int sceNetApctlTerm(void) { assert(!up && live_apctl && !live_resolver); if (result()) return -1; live_apctl=0; ++terminations[1]; return 0; }
int sceNetInetTerm(void) { assert(live_inet && !live_apctl); if (result()) return -1; live_inet=0; ++terminations[2]; return 0; }
int sceNetTerm(void) { assert(live_net && !live_inet); if (result()) return -1; live_net=0; ++terminations[3]; return 0; }
int sceNetApctlConnect(int p) { assert(live_resolver && !up && !draining); if (result()) return -1; ++connected; last_profile=p; up=1; if(reconnect_no_ip) no_ip=1; return 0; }
int sceNetApctlDisconnect(void) { if (result()) return -1; draining=2; return 0; }
int sceNetApctlGetState(int *s)
{
    if (result()) return -1;
    if (draining && !stuck && !--draining) up=0;
    *s=up ? (no_ip ? 1 : 4) : 0; return 0;
}
static void reset(void)
{
    memset(&network,0,sizeof(network)); saved_profile=borrowed_profile=-1; borrowed=0;
    fail=step=stopping=up=draining=stuck=connected=last_profile=no_ip=0; stop_at=0;
    live_net=live_inet=live_apctl=live_resolver=modules=0; ticks=0;
    memset(terminations,0,sizeof(terminations));
    profile_error=reconnect_no_ip=0; profile_change_at=0;
}
static void xmb(void)
{ up=1; live_net=live_inet=live_apctl=live_resolver=1; modules=2; }
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
    reset(); xmb();
    assert(!prox_network_borrow()); base=step;
    assert(prox_network_begin()<0 && !prox_network_retry(0) && prox_network_borrow()<0);
    assert(!prox_network_end() && step==base+6 && up && modules==2 && connected==1 && last_profile==7);
    assert(live_net && live_inet && live_apctl && live_resolver && !draining);
    for(i=0;i<4;++i) assert(!terminations[i]);
    assert(!prox_network_borrow() && !prox_network_end());
    reset(); assert(prox_network_borrow()<0 && !borrowed);
    reset(); up=1; stopping=1; assert(prox_network_borrow()<0 && !borrowed);
    reset(); up=1; fail=1; assert(prox_network_borrow()<0 && !borrowed);
    reset(); xmb(); assert(!prox_network_borrow()); base=step; stopping=1;
    assert(prox_network_retry(0)<0 && step==base); stopping=0; assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); base=step; ticks=90000001u;
    assert(prox_network_retry(0)<0 && step==base); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); no_ip=1;
    assert(prox_network_retry(0)<0 && up && !draining && !connected); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); fail=step+1;
    assert(prox_network_retry(0)<0 && up && !draining); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); ticks=0xfffff000u;
    { unsigned int start=ticks; ticks+=20000u; assert(!prox_network_retry(start)); }
    assert(!prox_network_end() && up && !draining && connected==1);
    reset(); xmb(); profile_error=1; assert(!prox_network_borrow()); base=step;
    assert(prox_network_retry(0)<0 && step==base && up && !connected); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); stuck=1;
    assert(prox_network_retry(0)<0 && ticks==5000000u && !connected); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); stop_at=20000;
    assert(prox_network_retry(0)<0 && ticks==20000u && !connected); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); profile_change_at=20000;
    assert(prox_network_retry(0)<0 && !connected); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); reconnect_no_ip=1;
    assert(prox_network_retry(0)<0 && ticks==20020000u && connected==1); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); reconnect_no_ip=1; stop_at=60000;
    assert(prox_network_retry(0)<0 && ticks==60000u && connected==1); assert(!prox_network_end());
    reset(); xmb(); assert(!prox_network_borrow()); no_ip=0;
    /* Each platform failure stops recovery without terminating shared owners. */
    for(i=1;i<=6;++i) {
        reset(); xmb(); assert(!prox_network_borrow()); fail=step+i;
        assert(prox_network_retry(0)<0 && modules==2 && live_net && live_resolver);
        assert(!terminations[0] && !terminations[1] && !terminations[2] && !terminations[3]);
        assert(!prox_network_end());
    }
    puts("PASS: owned session retry; native same-profile disconnect/state-zero/reconnect, deadlines/cancel/failures and shared module lifetime");
    return 0;
}
