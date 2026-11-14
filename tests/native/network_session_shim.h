#ifndef PROX_SESSION_SHIM_H
#define PROX_SESSION_SHIM_H
#include "network_close_shim.h"
#define PSP_NET_APCTL_STATE_GOT_IP 4
int sceUtilityLoadNetModule(int);
int sceUtilityCheckNetParam(int);
int sceNetInit(int,int,int,int,int);
int sceNetInetInit(void);
int sceNetApctlInit(int,int);
int sceNetResolverInit(void);
int sceNetApctlConnect(int);
#endif
