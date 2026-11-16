/* Download the exact consented contract; stage only verified allowed files.
 * No native legacy writer, flash or implicit second confirmation. GPL-3.0-or-later. */
#include "native_download.h"
#include "transport.h"
#include "stage.h"
#include "policy.h"
#include "bridge.h"
#include "prox_product.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned char *data; size_t used,capacity; } ManifestBytes;
typedef struct { FILE *file; unsigned int received,total; ProxNativeProgress progress; void *context; } PackageSink;
static int manifest_sink(const unsigned char *data,size_t size,void *arg)
{
    ManifestBytes *bytes=arg;
    if(size>bytes->capacity-bytes->used) return -1;
    memcpy(bytes->data+bytes->used,data,size); bytes->used+=size; return 0;
}
static int package_sink(const unsigned char *data,size_t size,void *arg)
{
    PackageSink *sink=arg;
    if(prox_stopping() || size>sink->total-sink->received || fwrite(data,1,size,sink->file)!=size) return -1;
    sink->received+=(unsigned int)size;
    return sink->progress?sink->progress(sink->received,sink->total,sink->context):0;
}
static void hash_text(const unsigned char data[32],char out[65])
{ int i; for(i=0;i<32;++i) snprintf(out+2*i,3,"%02x",data[i]); }
static int live_matches(const unsigned int running[3],unsigned int model)
{
    unsigned int live[3]; int i;
    for(i=0;i<3;++i) live[i]=(unsigned int)proxUpdateGetState(i+1);
    return proxUpdateGetState(0)==1 && proxUpdateGetState(4)==0x06060110 &&
           proxUpdateGetState(5)==(int)model && !prox_compare(live,running);
}
int prox_native_download(const char *directory,const char *ca,const unsigned int running[3],
                         unsigned int model,const ProxCatalogOffer *offer,ProxJob *job)
{ return prox_native_download_observed(directory,ca,running,model,offer,job,NULL,NULL); }
int prox_native_download_observed(const char *directory,const char *ca,const unsigned int running[3],
                         unsigned int model,const ProxCatalogOffer *offer,ProxJob *job,
                         ProxNativeProgress progress,void *context)
{
    PackageSink sink={0};
    ManifestBytes bytes={0}; PjToken *tokens=NULL; Pj json; ProxManifest manifest;
    ProxResponse response; unsigned int client[3]; int rc=-1,i,closed;
    unsigned long long required; char hash[65],manifest_path[256],zip_path[256]; FILE *file=NULL;
    if(!job) return -1;
    memset(job,0,sizeof(*job));
    if(!directory || !ca || !running || !offer || !prox_hash_text(offer->manifest_sha256) ||
       prox_version(PROX_PRODUCT_VERSION,client) || prox_compare(client,running) ||
       prox_stopping() || !live_matches(running,model)) return -1;
    bytes.capacity=32768; bytes.data=malloc(bytes.capacity); tokens=calloc(8192,sizeof(*tokens));
    if(!bytes.data || !tokens) goto done;
    memset(&response,0,sizeof(response));
    if(prox_https(offer->release.manifest.url,ca,bytes.capacity,manifest_sink,&bytes,&response) ||
       response.status!=200 || bytes.used!=offer->release.manifest.size || response.bytes!=bytes.used || prox_stopping()) goto done;
    hash_text(response.sha256,hash);
    if(strcmp(hash,offer->manifest_sha256) ||
       (offer->release.manifest.digest[0] && strcmp(hash,offer->release.manifest.digest)) ||
       pj_parse(&json,(char *)bytes.data,bytes.used,tokens,8192) ||
       prox_manifest(&json,&offer->release,running,client,model,&manifest) ||
       strcmp(manifest.version,offer->manifest.version) || strcmp(manifest.build_id,offer->manifest.build_id) ||
       strcmp(manifest.package_sha256,offer->manifest.package_sha256) ||
       manifest.package_size!=offer->manifest.package_size) {
        prox_log("Native download: consented manifest changed or failed validation"); goto done;
    }
    required=(unsigned long long)manifest.package_size+2u*1024u*1024u;
    for(i=0;i<PROX_UPDATE_FILES;++i) required+=2ull*manifest.files[i].size;
    if(!prox_power_ready() || prox_storage_space(directory,required) || prox_stopping() || !live_matches(running,model)) goto done;
    if(prox_new_stage(job->root,directory,manifest.build_id)) goto done;
    strcpy(job->source_version,PROX_PRODUCT_VERSION);
    if(prox_stage_dirs(job->root) ||
       snprintf(manifest_path,sizeof(manifest_path),"%s/pro-x-update.json",job->root)>=(int)sizeof(manifest_path) ||
       snprintf(zip_path,sizeof(zip_path),"%s/update.part",job->root)>=(int)sizeof(zip_path)) goto done;
    file=fopen(manifest_path,"wb"); if(!file) goto done;
    if(fwrite(bytes.data,1,bytes.used,file)!=bytes.used) goto done;
    closed=fclose(file); file=NULL; if(closed) goto done;
    strcpy(job->manifest_sha256,hash);
    if(prox_file_check(manifest_path,(unsigned int)bytes.used,hash,0) || prox_stopping()) goto done;
    file=fopen(zip_path,"wb"); if(!file) goto done;
    memset(&response,0,sizeof(response));
    sink.file=file; sink.total=manifest.package_size; sink.progress=progress; sink.context=context;
    closed=prox_https(offer->release.package.url,ca,manifest.package_size,package_sink,&sink,&response);
    if(fclose(file)) closed=-1;
    file=NULL; hash_text(response.sha256,hash);
    if(closed || response.status!=200 || response.bytes!=manifest.package_size ||
       strcmp(hash,manifest.package_sha256) || prox_stopping()) {
        prox_log("Native download: package request/size/SHA-256 failed"); goto done;
    }
    if(prox_zip_stage(zip_path,job->root,&manifest) || remove(zip_path) ||
       prox_stopping() || !live_matches(running,model) || !prox_power_ready() ||
       prox_stage_check(job->root,&manifest)) goto done;
    prox_log("Native download verified private stage; installer handoff remains separately gated"); rc=0;
done:
    if(file) fclose(file);
    if(rc && job->root[0]) { prox_stage_remove(job->root); memset(job,0,sizeof(*job)); }
    free(tokens); free(bytes.data); return rc;
}
