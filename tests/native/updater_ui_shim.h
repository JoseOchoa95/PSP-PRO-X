#ifndef UPDATER_UI_SHIM_H
#define UPDATER_UI_SHIM_H
#include <stddef.h>
typedef unsigned int SceSize;
typedef struct { unsigned int Buttons; } SceCtrlData;
typedef union { char asString[128]; } netData;
typedef struct { int unused; } SceIoStat;
#define PSP_MODULE_INFO(a,b,c,d)
#define PSP_MAIN_THREAD_ATTR(a)
#define PSP_MAIN_THREAD_STACK_SIZE_KB(a)
#define PSP_HEAP_SIZE_KB(a)
#define PSP_THREAD_ATTR_USER 0
#define PSP_CTRL_UP 0x10
#define PSP_CTRL_DOWN 0x40
#define PSP_CTRL_CIRCLE 0x2000
#define PSP_CTRL_CROSS 0x4000
#define PSP_NETPARAM_NAME 0
#define PSP_NET_APCTL_STATE_GOT_IP 4
#define PSP_NET_MODULE_COMMON 1
#define PSP_NET_MODULE_INET 2
#define PROX_BUILD_ID "native-ui-test"
unsigned int sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned int);
int sceCtrlPeekBufferPositive(SceCtrlData *, int);
int pspDebugScreenInit(void);
int pspDebugScreenPrintf(const char *, ...);
int sceKernelCreateCallback(const char *, int (*)(int,int,void *), void *);
int sceKernelRegisterExitCallback(int);
int sceKernelSleepThreadCB(void);
int sceKernelCreateThread(const char *, int (*)(SceSize,void *), int, int, int, void *);
int sceKernelStartThread(int, unsigned int, void *);
int sceUtilityCheckNetParam(int);
int sceUtilityGetNetParam(int, int, netData *);
int sceUtilityLoadNetModule(int);
int sceUtilityUnloadNetModule(int);
int sceNetInit(int, int, int, int, int);
int sceNetTerm(void);
int sceNetInetInit(void);
int sceNetInetTerm(void);
int sceNetApctlInit(int, int);
int sceNetApctlTerm(void);
int sceNetApctlConnect(int);
int sceNetApctlDisconnect(void);
int sceNetApctlGetState(int *);
int sceNetResolverInit(void);
int sceNetResolverTerm(void);
int sceIoGetstat(const char *, SceIoStat *);
int sctrlKernelExitVSH(void *);
int sceKernelExitGame(void);
#endif
