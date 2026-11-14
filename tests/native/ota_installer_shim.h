#ifndef PROX_OTA_INSTALLER_SHIM_H
#define PROX_OTA_INSTALLER_SHIM_H
typedef struct { unsigned int Buttons; } SceCtrlData;
#define PSP_CTRL_RTRIGGER 0x200
int sceCtrlPeekBufferPositive(SceCtrlData *,int);
int sceIoChdir(const char *);
int sceIoRemove(const char *);
int scePowerLock(int);
int scePowerUnlock(int);
int pspDebugScreenPrintf(const char *,...);
#endif
