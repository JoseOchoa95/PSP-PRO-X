/* Native frontend reuses the production HTTPS and release/manifest validators.
 * Bounded query only; no download confirmation, package write or installer.
 * GPL-3.0-or-later. */
#include "catalog.h"
#include "transport.h"
#include "policy.h"
#include "prox_product.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { char *data; size_t size,capacity; } Memory;
static int sink(const unsigned char *data,size_t size,void *context)
{
    Memory *m=context;
    if(size>m->capacity-m->size) return -1;
    memcpy(m->data+m->size,data,size); m->size+=size; return 0;
}
static int request(const char *url,const char *ca,Memory *m,ProxResponse *response)
{
    m->size=0;
    if(prox_stopping() || prox_https(url,ca,m->capacity,sink,m,response) || prox_stopping()) return -1;
    if(response->status!=200) {
        prox_log("Native catalog HTTP %d; query failed, not an up-to-date decision",response->status);
        return -1;
    }
    return 0;
}
int prox_catalog_query(const char *ca,const unsigned int running[3],unsigned int model,ProxCatalogOffer *offer)
{
    Memory memory={0}; Pj json; PjToken *tokens=NULL; ProxRelease *releases=NULL;
    ProxResponse response; unsigned int client[3];
    int page,count=0,finished=0,i,k,result=-1; char url[256],hash[65];
    if(!offer) return -1;
    memset(offer,0,sizeof(*offer));
    if(!ca || !running || prox_version(PROX_PRODUCT_VERSION,client) || prox_compare(client,running)) {
        prox_log("Native client and running product differ; use a matching manual build"); return -1;
    }
    memory.capacity=256*1024; memory.data=malloc(memory.capacity);
    tokens=calloc(8192,sizeof(*tokens)); releases=calloc(50,sizeof(*releases));
    if(!memory.data || !tokens || !releases) { prox_log("Native catalog bounded allocation failed"); goto done; }
    prox_log("REAL GitHub catalog query; installed %u.%u.%u; stable flags/version/contracts enforced",
             running[0],running[1],running[2]);
    for(page=1;page<=5;++page) {
        snprintf(url,sizeof(url),"https://api.github.com/repos/JoseOchoa95/PSP-PRO-X/releases?per_page=10&page=%d",page);
        if(request(url,ca,&memory,&response) || pj_parse(&json,memory.data,memory.size,tokens,8192) ||
           json.tokens[0].type!=PJ_ARRAY || json.tokens[0].count>10) goto done;
        ++offer->pages;
        for(i=1;i<json.tokens[0].next;i=json.tokens[i].next) {
            ProxRelease candidate;
            int rc=prox_release(&json,i,running,&candidate);
            if(rc<0) { prox_log("Native catalog invalid/ambiguous release metadata"); goto done; }
            if(rc==1) {
                if(count==50) goto done;
                for(k=0;k<count;++k) if(!prox_compare(releases[k].numbers,candidate.numbers)) goto done;
                releases[count++]=candidate;
            }
        }
        if(json.tokens[0].count<10) { finished=1; break; }
        if(prox_stopping()) goto done;
    }
    if(!finished || prox_stopping()) { prox_log("Native catalog incomplete/cancelled; no version verdict"); goto done; }
    offer->candidates=(unsigned int)count;
    for(i=0;i<count;++i) for(k=i+1;k<count;++k)
        if(prox_compare(releases[k].numbers,releases[i].numbers)>0) {
            ProxRelease swap=releases[i]; releases[i]=releases[k]; releases[k]=swap;
        }
    memory.capacity=32768;
    for(i=0;i<count;++i) {
        ProxRelease *r=&releases[i];
        if(request(r->manifest.url,ca,&memory,&response)) goto done;
        for(k=0;k<32;++k) snprintf(hash+2*k,3,"%02x",response.sha256[k]);
        if(memory.size!=r->manifest.size || (r->manifest.digest[0] && strcmp(hash,r->manifest.digest)) ||
           pj_parse(&json,memory.data,memory.size,tokens,8192)) goto done;
        if(prox_manifest(&json,r,running,client,model,&offer->manifest)) {
            ++offer->rejected; prox_log("Native catalog excluded %s: invalid/incompatible contract",r->tag); continue;
        }
        if(prox_stopping()) goto done;
        offer->release=*r;
        strcpy(offer->manifest_sha256,hash);
        prox_log("REAL compatible newer PRO-X %s; package %u bytes (query only)",
                 offer->manifest.version,offer->manifest.package_size);
        result=1; goto done;
    }
    if(offer->rejected) { prox_log("Native contracts rejected; not an up-to-date decision"); goto done; }
    if(prox_stopping()) goto done;
    prox_log("REAL validated catalog complete: pages=%u candidates=%u; no newer compatible stable version",
             offer->pages,offer->candidates);
    result=0;
done:
    free(tokens); free(releases); free(memory.data); return result;
}
