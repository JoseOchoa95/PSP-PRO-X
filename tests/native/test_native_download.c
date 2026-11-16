/* Exercise native staging with fixture transport, no UI/flash/launch API. */
#include "native_download.h"
#include "prox_product.h"
#include "transport.h"
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
static const char *fixture,*mode;
static int requests,downloaded,power_calls;
static unsigned int running[3];
int prox_stopping(void)
{ return !strcmp(mode,"cancel") || (!strcmp(mode,"cancel-package") && downloaded); }
void prox_log(const char *f,...)
{ va_list a; va_start(a,f); vprintf(f,a); va_end(a); putchar('\n'); }
int proxUpdateGetState(int s)
{
    const int state[]={1,0,3,2,0x06060110,1};
    if(s>=1 && s<=3) return (int)running[s-1];
    if(s==5 && (!strcmp(mode,"changed-before") || (!strcmp(mode,"changed-after") && downloaded))) return 0;
    return s<6?state[s]:-1;
}
int prox_power_ready(void)
{ ++power_calls; return strcmp(mode,"no-power") && (strcmp(mode,"power-lost") || power_calls==1); }
int prox_storage_space(const char *p,unsigned long long required)
{ (void)p; return !required || !strcmp(mode,"no-space")?-1:0; }
int prox_new_stage(char out[192],const char *directory,const char *id)
{
    (void)id;
    if(!strcmp(mode,"stage-failure") || snprintf(out,192,"%s/stage",directory)>=192) { out[0]=0; return -1; }
    return mkdir(out,0700);
}
int prox_https(const char *url,const char *ca,size_t limit,ProxSink sink,void *arg,ProxResponse *r)
{
    char path[1024]; FILE *f; unsigned char buffer[4096]; size_t n; int rc=-1,package;
    mbedtls_sha256_context hash;
    (void)ca; ++requests; memset(r,0,sizeof(*r)); r->status=200;
    package=strstr(url,"/pro-x-update.json")==NULL;
    if((package && !strcmp(mode,"package-http")) || (!package && !strcmp(mode,"manifest-http"))) { r->status=404; return 0; }
    if(!strcmp(mode,"network-error")) return -1;
    if(package) downloaded=1;
    snprintf(path,sizeof(path),"%s/%s",fixture,package?"package.zip":"manifest.json");
    f=fopen(path,"rb"); if(!f) return -1;
    mbedtls_sha256_init(&hash); mbedtls_sha256_starts(&hash,0);
    while((n=fread(buffer,1,sizeof(buffer),f))) {
        if(n>limit-r->bytes || sink(buffer,n,arg)) goto done;
        mbedtls_sha256_update(&hash,buffer,n); r->bytes+=n;
    }
    if(ferror(f)) goto done;
    mbedtls_sha256_finish(&hash,r->sha256);
    if(package && !strcmp(mode,"bad-package-hash")) r->sha256[0]^=1;
    if(package && !strcmp(mode,"bad-package-size")) ++r->bytes;
    rc=0;
done:
    mbedtls_sha256_free(&hash); fclose(f); return rc;
}
static int read_json(const char *name,char *data,size_t capacity,size_t *size)
{
    char path[1024]; FILE *f;
    snprintf(path,sizeof(path),"%s/%s",fixture,name); f=fopen(path,"rb"); if(!f) return -1;
    *size=fread(data,1,capacity,f); fclose(f); return *size==capacity?-1:0;
}
static unsigned int observed;
static int observe(unsigned int received,unsigned int total,void *context)
{
    if(context!=(void *)&observed || received<=observed || received>total) abort();
    observed=received;
    return !strcmp(mode,"observer-cancel")?-1:0;
}
int main(int argc,char **argv)
{
    char data[32768]; PjToken tokens[8192]; Pj json; size_t size;
    ProxCatalogOffer offer; ProxJob job; unsigned char hash[32]; int i,rc;
    if(argc!=4) return 3;
    fixture=argv[1]; mode=argv[3]; memset(&offer,0,sizeof(offer));
    if(sscanf(PROX_PRODUCT_VERSION,"%u.%u.%u",running,running+1,running+2)!=3) return 3;
    if(read_json("catalog.json",data,sizeof(data),&size) || pj_parse(&json,data,size,tokens,8192) ||
       prox_release(&json,1,running,&offer.release)!=1 ||
       read_json("manifest.json",data,sizeof(data),&size) || pj_parse(&json,data,size,tokens,8192) ||
       prox_manifest(&json,&offer.release,running,running,1,&offer.manifest)) return 3;
    mbedtls_sha256((unsigned char *)data,size,hash,0);
    for(i=0;i<32;++i) snprintf(offer.manifest_sha256+2*i,3,"%02x",hash[i]);
    if(!strcmp(mode,"snapshot-changed")) offer.manifest_sha256[0]=offer.manifest_sha256[0]=='0'?'1':'0';
    if(!strcmp(mode,"missing-hash")) offer.manifest_sha256[0]=0;
    rc=prox_native_download_observed(argv[2],"fixture CA",running,1,&offer,&job,observe,&observed);
    if(!rc && observed!=offer.manifest.package_size) abort();
    printf("STAGE result=%d requests=%d downloaded=%d power=%d root=%s\n",rc,requests,downloaded,power_calls,job.root);
    return rc?1:0;
}
