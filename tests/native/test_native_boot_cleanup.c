/* Production delayed XMB startup probe, with no removable storage or flash. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pspkernel.h"
#include "native_query_provider.h"
#include "prox_native_install.h"
#include "prox_resident_query.h"

static int (*entry)(SceSize,void *);
static int irq,waits,probes,started,requested,exited;
static const char *scenario;

unsigned int pspSdkDisableInterrupts(void) { assert(!irq);irq=1;return 7; }
void pspSdkEnableInterrupts(unsigned int mask) { assert(irq && mask==7);irq=0; }
int sceKernelDelayThread(unsigned int duration)
{
    assert(!irq && duration==1000000 && waits<10);
    ++waits;
    return 0;
}
int sceIoGetstat(const char *path,SceIoStat *stat)
{
    int ms=!strcmp(path,PROX_RESIDENT_MS_REPORTS "/" PROX_DEDICATED_COMMITTED_NAME);
    int ef=!strcmp(path,PROX_RESIDENT_EF_REPORTS "/" PROX_DEDICATED_COMMITTED_NAME);
    assert(!irq && waits && (ms || ef) && !requested);
    ++probes;
    if(ms && !strcmp(scenario,"directory")) {
        stat->st_mode=2;
        return 0;
    }
    if(waits==3 && ((ms && !strcmp(scenario,"card")) || (ef && !strcmp(scenario,"internal")))) {
        stat->st_mode=1;
        return 0;
    }
    return -1;
}
int prox_native_query_request_boot_cleanup(void)
{
    assert(!irq && waits==3 && !requested);
    ++requested;
    return 0;
}
int sceKernelCreateThread(const char *name,int (*callback)(SceSize,void *),int priority,
                          int stack,int attributes,void *option)
{
    assert(!irq && !entry && !strcmp(name,"PRO-X startup cleanup"));
    assert(priority==0x30 && stack==4096 && !attributes && !option);
    entry=callback;
    return 3;
}
int sceKernelStartThread(int id,unsigned int size,void *arg)
{
    assert(!irq && id==3 && !size && !arg && !started);
    ++started;
    return 0;
}
int sceKernelDeleteThread(int id) { (void)id;assert(0);return -1; }
int sceKernelExitDeleteThread(int result) { assert(!irq && !result);++exited;return 0; }

int main(int argc,char **argv)
{
    assert(argc==2);
    scenario=argv[1];
    prox_native_schedule_boot_cleanup();
    prox_native_schedule_boot_cleanup();
    assert(started==1 && !waits && !probes && !exited);
    assert(!entry(0,NULL) && exited==1);
    if(!strcmp(scenario,"card"))assert(waits==3 && probes==5 && requested==1);
    else if(!strcmp(scenario,"internal"))assert(waits==3 && probes==6 && requested==1);
    else assert(waits==10 && probes==20 && !requested);
    printf("PASS: XMB startup cleanup probe is asynchronous, bounded and record-gated (%s)\n",scenario);
}
