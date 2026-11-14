#ifndef PROX_STORAGE_POWER_SHIM_H
#define PROX_STORAGE_POWER_SHIM_H
int sceIoDevctl(const char *,unsigned int,void *,int,void *,int);
int scePowerIsPowerOnline(void);
int scePowerIsBatteryExist(void);
int scePowerGetBatteryLifePercent(void);
#endif
