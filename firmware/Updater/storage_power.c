/* SDK-only adapters: also loadable by the manual installer on official 6.61.
 * No private SystemControl imports are added to the installer. GPL-3.0-or-later. */
#ifdef PROX_STORAGE_NATIVE
#include "storage_power_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psppower.h>
#endif
#include "update.h"
#include "transport.h"
#include <string.h>
/* SCE_PR_GETDEV layout: PSPSDK user/pspiofilemgr_devctl.h. The pinned SDK
 * predates that header. Five 32-bit fields; pointer passed in the input buffer. */
typedef struct { unsigned int max_clusters,free_clusters,max_sectors; int sector_size,sectors_per_cluster; } Capacity;
_Static_assert(sizeof(Capacity) == 20, "PSP capacity structure ABI");
int prox_storage_space(const char *directory,unsigned long long required)
{
    Capacity capacity,*pointer=&capacity; char device[5]; unsigned long long free_bytes;
    if (strncmp(directory,"ms0:/",5) && strncmp(directory,"ef0:/",5)) return -1;
    memcpy(device,directory,4); device[4]=0; memset(&capacity,0,sizeof(capacity));
    if (sceIoDevctl(device,0x02425818,&pointer,sizeof(pointer),NULL,0) < 0 ||
        !capacity.max_clusters || capacity.free_clusters > capacity.max_clusters ||
        capacity.sector_size < 512 || capacity.sector_size > 65536 ||
        capacity.sectors_per_cluster < 1 || capacity.sectors_per_cluster > 65536) return -1;
    free_bytes=(unsigned long long)capacity.free_clusters*(unsigned int)capacity.sector_size*(unsigned int)capacity.sectors_per_cluster;
    prox_log("Storage free=%llu required=%llu bytes",free_bytes,required);
    return free_bytes >= required ? 0 : -1;
}
int prox_power_ready(void)
{
    int ac=scePowerIsPowerOnline(),battery=scePowerIsBatteryExist(),percent=scePowerGetBatteryLifePercent();
    prox_log("Installation power: AC=%d battery=%d charge=%d%%; AC and >=50%% required",ac,battery,percent);
    return ac == 1 && battery == 1 && percent >= 50 && percent <= 100;
}
