#define PROX_STORAGE_NATIVE
#include "../../firmware/Updater/storage_power.c"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
static Capacity sample={100,2,100,512,64};
static int io_result,ac=1,battery=1,percent=50;
void prox_log(const char *format,...) { (void)format; }
int sceIoDevctl(const char *device,unsigned int command,void *in,int length,void *out,int out_length)
{
    assert(!strcmp(device,"ms0:") || !strcmp(device,"ef0:")); assert(command == 0x02425818);
    assert(length == (int)sizeof(Capacity *) && out == NULL && out_length == 0);
    **(Capacity **)in=sample; return io_result;
}
int scePowerIsPowerOnline(void) { return ac; }
int scePowerIsBatteryExist(void) { return battery; }
int scePowerGetBatteryLifePercent(void) { return percent; }
int main(void)
{
    assert(!prox_storage_space("ms0:/PSP/GAME",65536));
    assert(prox_storage_space("ef0:/PSP/GAME",65537) < 0);
    assert(prox_storage_space("flash0:/",1) < 0);
    io_result=-1; assert(prox_storage_space("ms0:/",1)<0); io_result=0;
    sample.free_clusters=101; assert(prox_storage_space("ms0:/",1)<0); sample.free_clusters=2;
    sample.sector_size=0; assert(prox_storage_space("ms0:/",1)<0); sample.sector_size=512;
    sample.sectors_per_cluster=-1; assert(prox_storage_space("ms0:/",1)<0);
    sample.sectors_per_cluster=65536; sample.sector_size=65536; sample.free_clusters=sample.max_clusters=UINT_MAX;
    assert(prox_storage_space("ms0:/",ULLONG_MAX)<0);
    assert(prox_power_ready()); percent=49; assert(!prox_power_ready());
    percent=-1; assert(!prox_power_ready()); percent=101; assert(!prox_power_ready());
    percent=100; ac=0; assert(!prox_power_ready()); ac=1; battery=0; assert(!prox_power_ready());
    battery=1; assert(prox_power_ready());
    puts("PASS: storage-capacity ABI/64-bit bounds, insufficient/unknown space, AC/battery and charge thresholds");
    return 0;
}
