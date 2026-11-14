/* Public GitHub catalog -> compatible stable contract -> private verified stage.
 * Network and extraction never write flash or replace the active executable.
 * GPL-3.0-or-later. */
#include "update.h"
#include "stage.h"
#include "transport.h"
#include "policy.h"
#include "bridge.h"
#include "prox_product.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t size, capacity; } Memory;
static int memory_sink(const unsigned char *data,size_t size,void *context)
{
    Memory *m=context;
    if (size > m->capacity-m->size) return -1;
    memcpy(m->data+m->size,data,size); m->size+=size; return 0;
}
static int file_sink(const unsigned char *data,size_t size,void *context)
{ return fwrite(data,1,size,(FILE *)context) == size ? 0 : -1; }
static void hash_text(const unsigned char bytes[32],char out[65])
{ int i; for(i=0;i<32;++i) snprintf(out+2*i,3,"%02x",bytes[i]); }
static int request(const char *url,const char *ca,Memory *memory,ProxResponse *response)
{
    memory->size=0;
    if (prox_https(url,ca,memory->capacity,memory_sink,memory,response)) return -1;
    if (response->status != 200) {
        prox_log("GitHub HTTP %d; no up-to-date decision or automatic retry",response->status);
        return -1;
    }
    return 0;
}
void prox_observe_install(const char *directory,const unsigned int running[3])
{
    char name[256],observed[256],data[160],version[40],build[32],phase[16],extra;
    unsigned int target[3]; FILE *f; size_t n; int written;
    if (snprintf(name,sizeof(name),"%s/installed.pending",directory) >= (int)sizeof(name)) return;
    f=fopen(name,"rb"); if (!f) return;
    n=fread(data,1,sizeof(data)-1,f); written=ferror(f); fclose(f); data[n]=0;
    if (written || n == sizeof(data)-1 || sscanf(data,"PROX-OTA-1\n%39[0-9.]\n%31[a-z0-9-]\n%15[a-z]\n%c",version,build,phase,&extra) != 3 ||
        prox_version(version,target) || strlen(build) != 18 || strncmp(build,"pro-x-",6)) {
        prox_log("Pending installation record is invalid; preserved for investigation"); return;
    }
    if (strcmp(phase,"committed")) { prox_log("Earlier installation was not committed; preserve its stage/install.txt for recovery"); return; }
    if (prox_compare(target,running)) { prox_log("Pending %s has not been confirmed by the running firmware",version); return; }
    if (snprintf(observed,sizeof(observed),"%s/installed.observed",directory) >= (int)sizeof(observed)) return;
    f=fopen(observed,"wb"); if (!f) return;
    written=fprintf(f,"PROX-OTA-1\ntarget=%s\nbuild=%s\nrunning=%u.%u.%u\nverification=running-product-version-after-restart\n",
                    version,build,running[0],running[1],running[2]);
    if (fclose(f) || written < 0) return;
    remove(name);
    prox_log("Post-restart running PRO-X version %s confirmed (not a module-build attestation)",version);
}
int prox_prepare_update(const char *directory,const char *ca,const unsigned int running[3],unsigned int model,ProxJob *job)
{
    Memory memory; Pj json; PjToken *tokens=NULL; ProxRelease *releases=NULL;
    ProxManifest manifest; ProxResponse response;
    unsigned int client[3],live[3]; int page,count=0,finished=0,rejected=0,i,k,result=-1;
    char url[256],manifest_path[256],zip_path[256],hash[65];
    unsigned long long required; FILE *file=NULL;
    memset(job,0,sizeof(*job)); memset(&memory,0,sizeof(memory));
    memory.capacity=256*1024; memory.data=malloc(memory.capacity);
    tokens=calloc(8192,sizeof(*tokens)); releases=calloc(50,sizeof(*releases));
    if (!memory.data || !tokens || !releases || prox_version(PROX_PRODUCT_VERSION,client)) {
        prox_log("Cannot allocate bounded update catalog buffers"); goto done;
    }
    if (prox_compare(client,running)) {
        prox_log("Client and running product versions differ; use the matching manual package first"); goto done;
    }
    strcpy(job->source_version,PROX_PRODUCT_VERSION);
    prox_log("Checking public PRO-X releases; draft/prerelease and downgrades excluded");
    for (page=1;page<=5;++page) {
        snprintf(url,sizeof(url),"https://api.github.com/repos/JoseOchoa95/PSP-PRO-X/releases?per_page=10&page=%d",page);
        if (request(url,ca,&memory,&response) || pj_parse(&json,memory.data,memory.size,tokens,8192) ||
            json.tokens[0].type != PJ_ARRAY || json.tokens[0].count > 10) {
            prox_log("Catalog request/JSON invalid; nothing offered"); goto done;
        }
        for (i=1;i<json.tokens[0].next;i=json.tokens[i].next) {
            ProxRelease candidate;
            int rc=prox_release(&json,i,running,&candidate);
            if (rc < 0) { prox_log("Invalid or ambiguous release asset metadata"); goto done; }
            if (rc == 1) {
                if (count == 50) goto done;
                for (k=0;k<count;++k) if (!prox_compare(releases[k].numbers,candidate.numbers)) {
                    prox_log("Duplicate product version in release catalog; retry manually"); goto done;
                }
                releases[count++]=candidate;
            }
        }
        if (json.tokens[0].count < 10) { finished=1; break; }
        if (prox_stopping()) goto done;
    }
    if (!finished) { prox_log("Catalog exceeds 50-release limit; no update decision"); goto done; }
    for (i=0;i<count;++i) for (k=i+1;k<count;++k)
        if (prox_compare(releases[k].numbers,releases[i].numbers) > 0) {
            ProxRelease swap=releases[i]; releases[i]=releases[k]; releases[k]=swap;
        }
    for (i=0;i<count;++i) {
        ProxRelease *r=&releases[i];
        memory.capacity=32768;
        if (request(r->manifest.url,ca,&memory,&response)) goto done;
        hash_text(response.sha256,hash);
        if (memory.size != r->manifest.size || (r->manifest.digest[0] && strcmp(hash,r->manifest.digest)) ||
            pj_parse(&json,memory.data,memory.size,tokens,8192)) {
            prox_log("Manifest size/digest/JSON validation failed; no update decision"); goto done;
        }
        if (prox_manifest(&json,r,running,client,model,&manifest)) {
            ++rejected; prox_log("Excluded %s: invalid/incompatible update contract",r->tag); continue;
        }
        prox_log("Compatible stable PRO-X %s; package %u bytes",manifest.version,manifest.package_size);
        prox_log("%s",manifest.summary);
        if (!prox_confirm("X downloads and verifies this update; O cancels")) goto done;
        required=manifest.package_size+2u*1024u*1024u;
        for (k=0;k<PROX_UPDATE_FILES;++k) required+=2ull*manifest.files[k].size;
        if (prox_storage_space(directory,required)) { prox_log("Cannot verify enough free storage; nothing installed"); goto done; }
        if (prox_new_stage(job->root,directory,manifest.build_id)) { prox_log("Cannot create a fresh private stage"); goto done; }
        if (prox_stage_dirs(job->root) || snprintf(manifest_path,sizeof(manifest_path),"%s/pro-x-update.json",job->root) >= (int)sizeof(manifest_path) ||
            snprintf(zip_path,sizeof(zip_path),"%s/update.part",job->root) >= (int)sizeof(zip_path)) goto done;
        file=fopen(manifest_path,"wb"); if (!file) goto done;
        if (fwrite(memory.data,1,memory.size,file) != memory.size) goto done;
        if (fclose(file)) { file=NULL; goto done; } file=NULL;
        strcpy(job->manifest_sha256,hash);
        if (prox_file_check(manifest_path,(unsigned int)memory.size,hash,0)) goto done;
        file=fopen(zip_path,"wb"); if (!file) goto done;
        k=prox_https(r->package.url,ca,manifest.package_size,file_sink,file,&response);
        if (fclose(file)) k=-1;
        file=NULL;
        hash_text(response.sha256,hash);
        if (k || response.status != 200 || response.bytes != manifest.package_size || strcmp(hash,manifest.package_sha256)) {
            prox_log("Package request/size/SHA-256 rejected; nothing installed"); goto done;
        }
        if (prox_zip_stage(zip_path,job->root,&manifest)) { prox_log("ZIP/files/PBP verification rejected; nothing installed"); goto done; }
        remove(zip_path);
        for (k=0;k<3;++k) live[k]=(unsigned int)proxUpdateGetState(k+1);
        if (proxUpdateGetState(0) != 1 || proxUpdateGetState(4) != 0x06060110 ||
            proxUpdateGetState(5) != (int)model || prox_compare(live,running) ||
            prox_manifest(&json,r,live,client,model,&manifest) || !prox_power_ready()) {
            prox_log("Live compatibility or power check failed; nothing installed"); goto done;
        }
        if (!prox_confirm("Package verified. X opens the PRO-X installer; O cancels")) goto done;
        if (prox_stopping() || !prox_power_ready() || prox_stage_check(job->root,&manifest)) goto done;
        prox_log("Verified private stage ready; installer will recheck before writing");
        result=1; goto done;
    }
    if (rejected) prox_log("No compatible update offered; rejected contracts are not an up-to-date result");
    else prox_log("No newer compatible stable update found in the checked release catalog");
    result=0;
done:
    if (file) fclose(file);
    if (result != 1 && job->root[0]) { prox_stage_remove(job->root); memset(job,0,sizeof(*job)); }
    free(releases); free(tokens); free(memory.data); return result;
}
