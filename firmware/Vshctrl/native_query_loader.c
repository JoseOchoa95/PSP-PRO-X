/* Load only the matching local PBP's user PRX. No game LoadExec or UI drawing.
 * Owned temporary file only; never replace the active executable. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspmscm.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "prox_native_query.h"
#include "native_query_provider.h"
#ifdef PROX_NATIVE_DEDICATED_OTA
#include "prox_native_install.h"
#endif
#ifdef PROX_NATIVE_RESIDENT_QUERY
#include "prox_resident_query.h"
int prox_native_resident_load(void);
#else
#include "prox_pbp_payload.h"
#endif
#ifdef PROX_NATIVE_OTA
#include "native_query_cache.h"
#endif
#if defined(PROX_NATIVE_UI_QUERY) && !defined(PROX_NATIVE_OTA) && !defined(PROX_NATIVE_OFFER_PREVIEW)
#include <pspsysmem_kernel.h>

static void report_loader_profile(const unsigned int *before, unsigned int elapsed)
{
    const unsigned int after[] = {
        sceKernelPartitionTotalFreeMemSize(1),
        sceKernelPartitionTotalFreeMemSize(2),
        sceKernelPartitionMaxFreeMemSize(2)
    };
    prox_native_query_event("profile loader elapsed us", (int)elapsed);
    prox_native_query_event("profile kernel free before bytes", (int)before[0]);
    prox_native_query_event("profile kernel free after bytes", (int)after[0]);
    prox_native_query_event("profile user free before bytes", (int)before[1]);
    prox_native_query_event("profile user free after bytes", (int)after[1]);
    prox_native_query_event("profile user largest before bytes", (int)before[2]);
    prox_native_query_event("profile user largest after bytes", (int)after[2]);
}
#endif
static volatile int loading;
static unsigned int lease_id;
int prox_native_query_is_loading(void) { return loading; }
static int load_service(SceSize count,void *arg)
{
#ifndef PROX_NATIVE_RESIDENT_QUERY
    static const char marker[]="PROX-NATIVE-QUERY-V3";
    const char *paths[]={"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP","ef0:/PSP/GAME/PROXUPDATE/EBOOT.PBP"};
    unsigned char header[52],buffer[16384]; char temporary[100]={0};
    unsigned int offset,length,copied=0,matched=0; int i=2,source=-1,target=-1;
    int created=0,attempted_start=0,found=0; SceIoStat stat; SceKernelLMOption option;
#endif
    const char *executable=NULL;
    char arguments[256],lease_text[16];
    unsigned int used,failure=PROX_NQ_CAUSE_CLIENT;
    int module=-1,status=-1,rc=-1,started=0;
#if defined(PROX_NATIVE_UI_QUERY) && !defined(PROX_NATIVE_OTA) && !defined(PROX_NATIVE_OFFER_PREVIEW)
    const unsigned int before[] = {
        sceKernelPartitionTotalFreeMemSize(1),
        sceKernelPartitionTotalFreeMemSize(2),
        sceKernelPartitionMaxFreeMemSize(2)
    };
    const unsigned int profile_start = sceKernelGetSystemTimeLow();
#endif
    (void)count; (void)arg;
#ifdef PROX_NATIVE_RESIDENT_QUERY
    executable=PROX_RESIDENT_WORKER_ARG;
    module=prox_native_resident_load();
    prox_native_query_event("resident flash0 image verified and loaded",module);
    if(module<0) goto done;
#else
#ifdef PROX_NATIVE_OTA
    module=prox_native_query_cached_load(&executable);
    if(module>=0) goto start;
#endif
    for(i=0;i<2;++i) if(sceIoGetstat(paths[i],&stat)>=0) break;
    if(i==2) {
        /* The SDK's hardware presence query distinguishes no card from a
         * missing client file. Internal ef0 storage does not require a card. */
        if(sceIoGetstat("ef0:/",&stat)<0 && MScmIsMediumInserted()==0) failure=PROX_NQ_CAUSE_STORAGE;
        goto done;
    }
    if(stat.st_size<92 || stat.st_size>16*1024*1024) goto done;
    source=sceIoOpen(paths[i],PSP_O_RDONLY,0);
    if(source<0 || sceIoRead(source,header,40)!=40 ||
       prox_pbp_payload(header,(unsigned int)stat.st_size,&offset,&length) ||
       sceIoLseek(source,offset,PSP_SEEK_SET)!=(int)offset || sceIoRead(source,header,52)!=52 ||
       memcmp(header,"\177ELF",4) || header[4]!=1 || header[5]!=1 || header[18]!=8 || header[19]) goto done;
    if(sceIoLseek(source,offset,PSP_SEEK_SET)!=(int)offset) goto done;
    /* Fixed prefixes plus eight hex digits fit the 100-byte destination.
     * Kernel libc provides sprintf, not the user-mode snprintf symbol. */
    sprintf(temporary,"%s:/PSP/GAME/PROXUPDATE/.native-%08x.prx",i?"ef0":"ms0",lease_id);
    target=sceIoOpen(temporary,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_EXCL,0600);
    if(target<0) goto done;
    created=1;
    while(copied<length) {
        unsigned int j,n=length-copied; if(n>sizeof(buffer)) n=sizeof(buffer);
        if(sceIoRead(source,buffer,n)!=(int)n || sceIoWrite(target,buffer,n)!=(int)n) goto done;
        for(j=0;j<n && !found;++j) {
            if(buffer[j]==(unsigned char)marker[matched]) ++matched;
            else matched=buffer[j]==(unsigned char)marker[0]?1u:0u;
            if(matched==sizeof(marker)-1) found=1;
        }
        copied+=n;
    }
    if(!found) { prox_native_query_event("native service marker missing",-1); goto done; }
    if(sceIoClose(target)<0) { target=-1; goto done; } target=-1;
    memset(&option,0,sizeof(option)); option.size=sizeof(option); option.access=1; option.position=1;
    module=sceKernelLoadModule(temporary,0,&option);
    prox_native_query_event("native service module load",module);
    if(module<0) goto done;
    executable=paths[i];
#ifdef PROX_NATIVE_OTA
start:
#endif
#endif
    used=strlen(executable)+1; memcpy(arguments,executable,used);
    memcpy(arguments+used,"--prox-native-query-v3",23); used+=23;
    sprintf(lease_text,"%08x",lease_id);
    memcpy(arguments+used,lease_text,9); used+=9;
#ifndef PROX_NATIVE_RESIDENT_QUERY
    attempted_start=1;
#endif
    rc=sceKernelStartModule(module,used,arguments,&status,NULL);
    prox_native_query_event("native service module start",rc);
    prox_native_query_event("native service module status",status);
    if(rc<0 || status<0) goto done;
    started=1;
done:
#ifndef PROX_NATIVE_RESIDENT_QUERY
    if(target>=0) sceIoClose(target);
    if(source>=0) sceIoClose(source);
    if(created) sceIoRemove(temporary);
#endif
#if defined(PROX_NATIVE_UI_QUERY) && !defined(PROX_NATIVE_OTA) && !defined(PROX_NATIVE_OFFER_PREVIEW)
    report_loader_profile(before, sceKernelGetSystemTimeLow() - profile_start);
#endif
#ifdef PROX_NATIVE_RESIDENT_QUERY
    if(module>=0 && !started) sceKernelUnloadModule(module);
#else
    if(module>=0 && !attempted_start) sceKernelUnloadModule(module);
#endif
    if(!started) {
#ifndef PROX_NATIVE_RESIDENT_QUERY
        if(i==0 && MScmIsMediumInserted()==0) failure=PROX_NQ_CAUSE_STORAGE;
#endif
        prox_native_query_load_failed_reason(failure);
    }
    loading=0; sceKernelExitDeleteThread(0); return 0;
}
int prox_native_query_start(unsigned int lease)
{
    SceUID thread;
    if(sceKernelFindModuleByName("ProxUpdater")) return 0;
    if(loading) return 0;
    lease_id=lease; loading=1;
    thread=sceKernelCreateThread("PRO-X native loader",load_service,0x18,24576,0,NULL);
    if(thread<0) { loading=0; return thread; }
    if(sceKernelStartThread(thread,0,NULL)<0) { sceKernelDeleteThread(thread); loading=0; return -1; }
    return 0;
}
#ifdef PROX_NATIVE_DEDICATED_OTA
static int boot_cleanup_thread(SceSize count,void *arg)
{
    (void)count;
    (void)arg;
    /* Give removable storage time to mount after the XMB module starts. */
    for (unsigned int attempt=0;attempt<10;++attempt) {
        SceIoStat stat;
        sceKernelDelayThread(1000000);
        if ((sceIoGetstat(PROX_RESIDENT_MS_REPORTS "/" PROX_DEDICATED_COMMITTED_NAME,&stat)>=0 &&
             FIO_S_ISREG(stat.st_mode)) ||
            (sceIoGetstat(PROX_RESIDENT_EF_REPORTS "/" PROX_DEDICATED_COMMITTED_NAME,&stat)>=0 &&
             FIO_S_ISREG(stat.st_mode))) {
            prox_native_query_request_boot_cleanup();
            break;
        }
    }
    sceKernelExitDeleteThread(0);
    return 0;
}

void prox_native_schedule_boot_cleanup(void)
{
    static int scheduled;
    unsigned int mask=pspSdkDisableInterrupts();
    if (scheduled) {
        pspSdkEnableInterrupts(mask);
        return;
    }
    scheduled=1;
    pspSdkEnableInterrupts(mask);
    SceUID thread=sceKernelCreateThread("PRO-X startup cleanup",boot_cleanup_thread,0x30,4096,0,NULL);
    if (thread>=0 && sceKernelStartThread(thread,0,NULL)<0)
        sceKernelDeleteThread(thread);
}
#endif
