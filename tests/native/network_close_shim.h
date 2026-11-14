#ifndef PROX_NETWORK_CLOSE_SHIM_H
#define PROX_NETWORK_CLOSE_SHIM_H
#define PSP_NET_APCTL_STATE_DISCONNECTED 0
#define PSP_NET_MODULE_COMMON 1
#define PSP_NET_MODULE_INET 2
unsigned int sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned int);
int sceNetResolverTerm(void);
int sceNetApctlGetState(int *);
int sceNetApctlDisconnect(void);
int sceNetApctlTerm(void);
int sceNetInetTerm(void);
int sceNetTerm(void);
int sceUtilityUnloadNetModule(int);
#endif
