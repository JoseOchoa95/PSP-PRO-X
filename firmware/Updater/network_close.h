/* Teardown completion gates installer handoff. GPL-3.0-or-later. */
#ifndef PROX_NETWORK_CLOSE_H
#define PROX_NETWORK_CLOSE_H
typedef struct { int common,inet_module,net,inet,apctl,resolver; } ProxNetworkState;
int prox_network_close_state(ProxNetworkState *state,int unload_modules);
int prox_network_close(int resolver,int apctl,int inet,int net,int inet_module,int common);
#endif
