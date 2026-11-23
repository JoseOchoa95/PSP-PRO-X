/* Authenticated target payloads as data; local matched engine, no LoadExec. */
#ifdef PROX_BACKGROUND_TARGET_TEST
#include "native_target_shim.h"
#else
#include <pspkernel.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#include "kubridge.h"
#include "mbedtls/sha256.h"
#include "native_engine.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_background.h"
#include "native_payload.h"
#include "native_signed.h"
#include "native_service.h"
#include "native_completion_text.h"
#include "prox_completion_preview.h"
#include "prox_pbp_payload.h"
#include "prox_product.h"
#include "stage.h"
#include "transport.h"
const char prox_background_identity[]="PROX-NATIVE-BACKGROUND-INSTALL-V1";
static int exchange(int action,ProxNativeInstallMessage *message)
{ return prox_native_install_exchange(action,message); }
static int digest(const unsigned char *bytes,unsigned int size,char out[65])
{
    unsigned char hash[32];unsigned int i;
    if(mbedtls_sha256(bytes,size,hash,0))return -1;
    for(i=0;i<32;++i)snprintf(out+i*2,3,"%02x",hash[i]);
    return 0;
}
static int put(const char *path,const void *bytes,unsigned int size)
{
    FILE *file=fopen(path,"wb");int rc;
    if(!file)return -1;
    rc=fwrite(bytes,1,size,file)==size?0:-1;
    if(fclose(file))rc=-1;
    return rc;
}
typedef struct { ProxNativeInstallJob *job;unsigned int created; } PayloadSink;
static int payload(unsigned int id,const unsigned char *bytes,unsigned int size,void *context)
{
    PayloadSink *sink=context;char path[256];FILE *file;int rc;
    if(id>=PROX_NI_PARTS || prox_stopping() ||
       snprintf(path,sizeof(path),"%s/native/%02u.prx",sink->job->root,id)>=(int)sizeof(path))return -1;
    file=fopen(path,"wb");if(!file)return -1;
    sink->created|=1u<<id;
    rc=fwrite(bytes,1,size,file)==size?0:-1;if(fclose(file))rc=-1;
    if(rc || digest(bytes,size,sink->job->parts[id].sha256))return -1;
    sink->job->parts[id].size=size;
    return prox_file_check(path,size,sink->job->parts[id].sha256,0);
}
void prox_native_background_discard(const char *root)
{
    unsigned int i;char path[256];
    for(i=0;i<PROX_NI_PARTS;++i) { snprintf(path,sizeof(path),"%s/native/%02u.prx",root,i);remove(path); }
    snprintf(path,sizeof(path),"%s/native/launcher.prx",root);remove(path);
    snprintf(path,sizeof(path),"%s/native/launcher.pbp",root);remove(path);
    snprintf(path,sizeof(path),"%s/native/engine.prx",root);remove(path);
    snprintf(path,sizeof(path),"%s/native",root);sceIoRmdir(path);
}
int prox_native_background_prepare(const ProxJob *job,const ProxManifest *manifest,unsigned int model,ProxNativeInstallJob *out)
{
    char path[256],hash[65];unsigned char *pbp=NULL;unsigned int offset,length;
    int created=0,rc=-1;FILE *file=NULL;const char *step="stage verification";
    PayloadSink sink={0};ProxNativePayloadScan scan={0};
    if(!job || !manifest || !out || manifest->files[0].size>2u*1024u*1024u ||
       !manifest->files[0].size || prox_stage_check(job->root,manifest) || prox_stopping())return -1;
    memset(out,0,sizeof(*out));strcpy(out->root,job->root);strcpy(out->manifest_sha256,job->manifest_sha256);
    strcpy(out->source_version,job->source_version);strcpy(out->target_version,manifest->version);
    strcpy(out->build_id,manifest->build_id);out->model=model;sink.job=out;scan.sink=payload;scan.context=&sink;
    step="target PBP read/hash";
    snprintf(path,sizeof(path),"%s/PROUPDATE/EBOOT.PBP",job->root);
    pbp=malloc(manifest->files[0].size);if(!pbp)goto done;
    file=fopen(path,"rb");if(!file)goto done;
    if(fread(pbp,1,manifest->files[0].size,file)!=manifest->files[0].size || fgetc(file)!=EOF || ferror(file))goto done;
    if(fclose(file)) { file=NULL;goto done; }file=NULL;
    if(digest(pbp,manifest->files[0].size,hash) || strcmp(hash,manifest->files[0].sha256) ||
       prox_pbp_payload(pbp,manifest->files[0].size,&offset,&length) || length<0x150 ||
       memcmp(pbp+offset,"~PSP",4) || prox_pbp_u32(pbp+offset+0x2c)!=length)goto done;
    step="private data directory";
    snprintf(path,sizeof(path),"%s/native",job->root);if(sceIoMkdir(path,0700)<0)goto done;created=1;
    /* Decode the pinned authenticated bytes directly. No target module load,
     * relocation, imports, permissions or start callbacks are involved. */
    step="authenticated target data decode";
    if(prox_native_signed_scan(&scan,pbp+offset,length))goto done;
    free(pbp);pbp=NULL;
    step="complete target payload set";
    if(prox_native_payload_done(&scan) || prox_stopping() || prox_stage_check(job->root,manifest))goto done;
    prox_log("Target %s extracted as data: ten PRO modules and matched rebooter; target launcher/installer never loaded or started",manifest->version);
    rc=0;
done:
    if(file)fclose(file);
    free(pbp);
    if(rc)prox_log("Background preparation failed at %s; found=0x%x containers=%u; no firmware writes",step,scan.found,scan.containers);
    if(rc && created)prox_native_background_discard(job->root);
    return rc;
}
int prox_native_background_launch(const char *directory,const ProxNativeInstallJob *job)
{
    ProxNativeInstallMessage message;char path[256],args[400],number[9];size_t used;
    unsigned short text[PROX_COMPLETION_UNITS];int language=-1,count,module,status=-1,rc;FILE *file;
    if(!job || prox_stopping() || sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,&language)<0 || language<0 || language>11)return -1;
    snprintf(path,sizeof(path),"%s/native-completion.rco",directory);
    if(prox_file_check(path,PROX_COMPLETION_RCO_SIZE,PROX_COMPLETION_RCO_SHA256,0))return -1;
    file=fopen(path,"rb");if(!file)return -1;count=prox_completion_read_text(file,(unsigned int)language,text,PROX_COMPLETION_UNITS);
    rc=fclose(file);if(count<0 || rc || prox_native_completion_prepare(text,(unsigned int)count))return -1;
    snprintf(path,sizeof(path),"%s/native/engine.prx",job->root);
    if(put(path,prox_native_engine,PROX_NATIVE_ENGINE_SIZE) || prox_file_check(path,PROX_NATIVE_ENGINE_SIZE,PROX_NATIVE_ENGINE_SHA256,0))return -1;
    memset(&message,0,sizeof(message));message.job=*job;if(exchange(PROX_NI_ARM,&message))return -1;
    module=kuKernelLoadModule(path,0,NULL);if(module<0) { exchange(PROX_NI_FAIL,&message);return -1; }
    message.module=module;if(exchange(PROX_NI_BIND,&message))return -1;
    used=strlen(path)+1;memcpy(args,path,used);
    memcpy(args+used,PROX_NI_ARGUMENT,sizeof(PROX_NI_ARGUMENT));used+=sizeof(PROX_NI_ARGUMENT);
    snprintf(number,sizeof(number),"%08x",message.lease);memcpy(args+used,number,9);used+=9;
    snprintf(number,sizeof(number),"%08x",message.generation);memcpy(args+used,number,9);used+=9;
    snprintf(number,sizeof(number),"%08x",(unsigned int)module);memcpy(args+used,number,9);used+=9;
    prox_log("Verified target background engine starts; no LoadExec or target installer UI");
    if(prox_native_handoff_close()) { exchange(PROX_NI_FAIL,&message);return -1; }
    rc=sceKernelStartModule(module,(unsigned int)used,args,&status,NULL);
    if(rc<0 || status<0) {
        if(exchange(PROX_NI_STATUS,&message) || message.phase<PROX_NI_CLAIMED) { exchange(PROX_NI_FAIL,&message);return -1; }
    }
    return 0;
}
int prox_native_background_wait(void)
{
    ProxNativeInstallMessage message;memset(&message,0,sizeof(message));
    for(;;) {
        if(exchange(PROX_NI_STATUS,&message))return -1;
        if(message.phase==PROX_NI_FAILED)return -1;
        if(message.phase<PROX_NI_WRITING && message.idle_us>25000000u) {
            (void)exchange(PROX_NI_FAIL,&message);return -1;
        }
        /* No timeout or cancellation can release the native page while real
         * writes/storage commit own it, or while completed UI awaits Cross. */
        sceKernelDelayThread(20000);
    }
}
