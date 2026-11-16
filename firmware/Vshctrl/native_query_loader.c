/* Load only the matching local PBP's user PRX. No game LoadExec or UI drawing.
 * Owned temporary file only; never replace the active executable. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspmscm.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "prox_pbp_payload.h"
#include "prox_native_query.h"
#include "native_query_provider.h"
#ifdef PROX_NATIVE_OTA
#include "native_query_cache.h"
#endif
static volatile int loading;
static unsigned int lease_id;
int prox_native_query_is_loading(void) { return loading; }
static int load_service(SceSize count,void *arg)
{
    static const char marker[]="PROX-NATIVE-QUERY-V3";
    const char *paths[]={"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP","ef0:/PSP/GAME/PROXUPDATE/EBOOT.PBP"};
    unsigned char header[52],buffer[16384]; char temporary[100]={0},arguments[256],lease_text[16];
    unsigned int offset,length,copied=0,matched=0,used,failure=PROX_NQ_CAUSE_CLIENT; int i=2,source=-1,target=-1,module=-1,status=-1,rc=-1;
    const char *executable=NULL;
    int created=0,started=0,attempted_start=0,found=0; SceIoStat stat; SceKernelLMOption option;
    (void)count; (void)arg;
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
    used=strlen(executable)+1; memcpy(arguments,executable,used);
    memcpy(arguments+used,"--prox-native-query-v3",23); used+=23;
    sprintf(lease_text,"%08x",lease_id);
    memcpy(arguments+used,lease_text,9); used+=9;
    attempted_start=1;
    rc=sceKernelStartModule(module,used,arguments,&status,NULL);
    prox_native_query_event("native service module start",rc);
    prox_native_query_event("native service module status",status);
    if(rc<0 || status<0) goto done;
    started=1;
done:
    if(target>=0) sceIoClose(target);
    if(source>=0) sceIoClose(source);
    if(created) sceIoRemove(temporary);
    if(module>=0 && !attempted_start) sceKernelUnloadModule(module);
    if(!started) {
        if(i==0 && MScmIsMediumInserted()==0) failure=PROX_NQ_CAUSE_STORAGE;
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
