/* Explicit opt-in no-flash fixture. Never linked into a normal OTA client. */
#ifdef PROX_INSTALL_LOADER_TEST
#include "install_preview_loader_shim.h"
#else
#include <pspkernel.h>
#include <psputility.h>
#include "kubridge.h"
#include "native_preview_hashes.h"
#endif
#include <stdio.h>
#include <string.h>
#include "native_download.h"
#include "native_service.h"
#include "stage.h"
#include "transport.h"
#include "prox_install_preview.h"
#include "native_completion_text.h"
#include "prox_completion_preview.h"
const char prox_install_preview_identity[]="PROX-INSTALL-PREVIEW-NO-OTA-NO-FLASH";
static int files(const char *directory)
{
    char path[160];
    if(!prox_install_preview_path(directory)) return -1;
    snprintf(path,sizeof(path),"%s/native-install-preview.prx",directory);
    if(prox_file_check(path,PROX_PREVIEW_MODULE_SIZE,PROX_PREVIEW_MODULE_SHA256,0)) return -1;
    snprintf(path,sizeof(path),"%s/native-preview-reboot.prx",directory);
    if(prox_file_check(path,PROX_PREVIEW_REBOOT_SIZE,PROX_PREVIEW_REBOOT_SHA256,0)) return -1;
    snprintf(path,sizeof(path),"%s/native-completion.rco",directory);
    return prox_file_check(path,PROX_COMPLETION_RCO_SIZE,PROX_COMPLETION_RCO_SHA256,0);
}
int prox_install_preview_download(const char *directory,ProxNativeProgress progress)
{
    unsigned int bytes;
    if(files(directory)) return -1;
    prox_log("DIAGNOSTIC: synthetic native progress; no GitHub download, stage or installation");
    for(bytes=32768;bytes<=PROX_INSTALL_PREVIEW_BYTES;bytes+=32768) {
        if(prox_native_is_cancelled() || progress(bytes,PROX_INSTALL_PREVIEW_BYTES,NULL)) return -1;
        sceKernelDelayThread(120000);
    }
    return 0;
}
int prox_install_preview_launch(const char *directory)
{
    char path[160],args[280]; size_t used; int module,rc,status=-1,language=-1,count;
    unsigned short text[PROX_COMPLETION_UNITS];FILE *resource;
    if(files(directory) || prox_native_is_cancelled()) return -1;
    if(sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,&language)<0 || language<0 || language>11) return -1;
    snprintf(path,sizeof(path),"%s/native-completion.rco",directory);
    resource=fopen(path,"rb");if(!resource) return -1;
    count=prox_completion_read_text(resource,(unsigned int)language,text,PROX_COMPLETION_UNITS);
    rc=fclose(resource);
    if(count<0 || rc || prox_native_completion_prepare(text,(unsigned int)count)) return -1;
    /* The worker closed its report before handoff. Do not log to that closed
     * descriptor; the probe owns the completion/input report from here. */
    snprintf(path,sizeof(path),"%s/native-install-preview.prx",directory);
    used=strlen(path)+1; memcpy(args,path,used);
    memcpy(args+used,PROX_INSTALL_PREVIEW_ARGUMENT,sizeof(PROX_INSTALL_PREVIEW_ARGUMENT)); used+=sizeof(PROX_INSTALL_PREVIEW_ARGUMENT);
    memcpy(args+used,directory,strlen(directory)+1); used+=strlen(directory)+1;
    module=kuKernelLoadModule(path,0,NULL);
    if(module<0) return -1;
    rc=sceKernelStartModule(module,(unsigned int)used,args,&status,NULL);
    if(rc<0 || status<0) return -1;
    /* The read-only module owns the short preview and existing CFW restart.
     * A returned/failed restart stays an error, never an installed update. */
    {
        unsigned int startup=0;
        for(;;) {
            int phase=prox_native_completion_pending();
            if(phase<0 || (phase<2 && ++startup>=1250)) return -1;
            /* No user deadline after completion. The probe's live polling
             * keeps input ownership healthy while either button is awaited. */
            sceKernelDelayThread(20000);
        }
    }
}
