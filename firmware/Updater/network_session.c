/* Fresh user-mode net/Inet/APCTL/resolver session for retry, preserving profile.
 * Physical WLAN switch and firmware drivers are not modified. GPL-3.0-or-later. */
#ifdef PROX_SESSION_NATIVE
#include "network_session_shim.h"
#else
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#endif
#include "network_session.h"
#include "network_close.h"
#include "transport.h"
static ProxNetworkState network;
static int saved_profile=-1;
static int stack_init(void)
{
    int rc;
#define INIT(call,flag) do { rc=(call); prox_log(#call " = 0x%08x",rc); if (rc < 0) return -1; network.flag=1; } while (0)
    INIT(sceNetInit(256*1024,0x30,4096,0x30,4096),net);
    INIT(sceNetInetInit(),inet);
    INIT(sceNetApctlInit(0x8000,0x30),apctl);
    INIT(sceNetResolverInit(),resolver);
#undef INIT
    return 0;
}
int prox_network_begin(void)
{
    int rc;
    saved_profile=-1;
    if (network.common || network.inet_module || network.net || network.inet || network.apctl || network.resolver) return -1;
    rc=sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON); prox_log("Load COMMON rc=0x%08x",rc);
    if (rc < 0) return -1;
    network.common=1;
    rc=sceUtilityLoadNetModule(PSP_NET_MODULE_INET); prox_log("Load INET rc=0x%08x",rc);
    if (rc < 0) return -1;
    network.inet_module=1;
    return stack_init();
}
int prox_network_end(void) { return prox_network_close_state(&network,1); }
static int connect_profile(int profile,unsigned int request_start,int bounded)
{
    unsigned int start=sceKernelGetSystemTimeLow();
    int state=-1,rc;
    if (profile < 1 || profile > 99 || !network.resolver ||
        sceUtilityCheckNetParam(profile) || prox_stopping()) return -1;
    if (bounded && (unsigned int)(start-request_start)>90000000u) return -1;
    saved_profile=profile;
    rc=sceNetApctlConnect(profile); prox_log("APCTL connect profile=%d rc=0x%08x",profile,rc);
    if (rc < 0) return -1;
    while ((unsigned int)(sceKernelGetSystemTimeLow()-start)<45000000u) {
        if (prox_stopping() || (bounded && (unsigned int)(sceKernelGetSystemTimeLow()-request_start)>90000000u)) return -1;
        rc=sceNetApctlGetState(&state);
        if (rc < 0) { prox_log("APCTL state failed rc=0x%08x",rc); return -1; }
        if (state == PSP_NET_APCTL_STATE_GOT_IP) { prox_log("IP ready (HTTPS still untested)"); return 0; }
        sceKernelDelayThread(20000);
    }
    prox_log("APCTL IP wait timed out; last state=%d",state); return -1;
}
int prox_network_connect(int profile) { return connect_profile(profile,0,0); }
int prox_network_retry(unsigned int request_start)
{
    int profile=saved_profile;
    if (profile < 1 || prox_stopping() ||
        (unsigned int)(sceKernelGetSystemTimeLow()-request_start)>90000000u) return -1;
    prox_log("Wi-Fi retry: close session and recreate net/Inet/APCTL/resolver; same profile=%d",profile);
    if (prox_network_close_state(&network,0)) return -1;
    if (prox_stopping() || (unsigned int)(sceKernelGetSystemTimeLow()-request_start)>90000000u) return -1;
    if (stack_init()) return -1;
    return connect_profile(profile,request_start,1);
}
