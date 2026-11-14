/* Never announce a closed network or unload its dependencies after a failed
 * termination. APCTL disconnect is asynchronous: observe DISCONNECTED first.
 * GPL-3.0-or-later. */
#ifdef PROX_NETWORK_NATIVE
#include "network_close_shim.h"
#else
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#endif
#include "network_close.h"
#include "transport.h"

static int disconnect(void)
{
    unsigned int start=sceKernelGetSystemTimeLow();
    int state=-1,rc=sceNetApctlGetState(&state);
    prox_log("Network close APCTL initial: rc=0x%08x state=%d",rc,state);
    if (rc < 0) return -1;
    if (state == PSP_NET_APCTL_STATE_DISCONNECTED) return 0;
    rc=sceNetApctlDisconnect(); prox_log("Network close disconnect rc=0x%08x",rc);
    if (rc < 0) return -1;
    while ((unsigned int)(sceKernelGetSystemTimeLow()-start) < 5000000u) {
        rc=sceNetApctlGetState(&state);
        if (rc < 0) { prox_log("Network close state read failed rc=0x%08x",rc); return -1; }
        if (state == PSP_NET_APCTL_STATE_DISCONNECTED) {
            prox_log("Network disconnected elapsed_ms=%u",(unsigned int)(sceKernelGetSystemTimeLow()-start)/1000); return 0;
        }
        sceKernelDelayThread(20000);
    }
    prox_log("Network disconnect watchdog expired; last APCTL state=%d",state); return -1;
}
int prox_network_close_state(ProxNetworkState *s,int unload_modules)
{
    int rc;
#define CLOSE(call) do { rc=(call); prox_log("Network close " #call " rc=0x%08x",rc); if (rc < 0) goto failed; } while (0)
    if (s->resolver) { CLOSE(sceNetResolverTerm()); s->resolver=0; }
    if (s->apctl) { if (disconnect()) goto failed; CLOSE(sceNetApctlTerm()); s->apctl=0; }
    if (s->inet) { CLOSE(sceNetInetTerm()); s->inet=0; }
    if (s->net) { CLOSE(sceNetTerm()); s->net=0; }
    if (unload_modules && s->inet_module) { CLOSE(sceUtilityUnloadNetModule(PSP_NET_MODULE_INET)); s->inet_module=0; }
    if (unload_modules && s->common) { CLOSE(sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON)); s->common=0; }
#undef CLOSE
    prox_log(unload_modules ? "Network teardown complete" : "Network session closed; modules retained for reconnect"); return 0;
failed:
    prox_log("Network teardown incomplete; dependent libraries kept loaded, installer handoff blocked");
    return -1;
}
int prox_network_close(int resolver,int apctl,int inet,int net,int inet_module,int common)
{
    ProxNetworkState state={common,inet_module,net,inet,apctl,resolver};
    return prox_network_close_state(&state,1);
}
