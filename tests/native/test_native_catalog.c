/* Fixture transport around the actual production catalog selector. */
#include "catalog.h"
#include "prox_product.h"
#include "transport.h"
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static const char *directory,*mode;
static int requests;
int prox_stopping(void) { return !strcmp(mode,"cancel"); }
void prox_log(const char *f,...) { va_list a; va_start(a,f); vprintf(f,a); va_end(a); putchar('\n'); }
int prox_https(const char *url,const char *ca,size_t cap,ProxSink sink,void *arg,ProxResponse *r)
{
    char name[1024]; FILE *f; unsigned char bytes[4096]; size_t n;
    mbedtls_sha256_context ctx; int rc=-1;
    (void)ca; ++requests; memset(r,0,sizeof(*r)); r->status=200;
    if(!strcmp(mode,"network")) return -1;
    if(!strcmp(mode,"http")) { r->status=403; return 0; }
    if(strstr(url,"api.github.com/")) snprintf(name,sizeof(name),"%s/catalog.json",directory);
    else if(strstr(url,"/pro-x-update.json")) snprintf(name,sizeof(name),"%s/manifest.json",directory);
    else return -1; /* No package request is allowed in a query. */
    f=fopen(name,"rb"); if(!f) return -1;
    mbedtls_sha256_init(&ctx); mbedtls_sha256_starts(&ctx,0);
    while((n=fread(bytes,1,sizeof(bytes),f))) {
        if(n>cap-r->bytes || sink(bytes,n,arg)) goto done;
        r->bytes+=n; mbedtls_sha256_update(&ctx,bytes,n);
    }
    if(ferror(f)) goto done;
    mbedtls_sha256_finish(&ctx,r->sha256); rc=0;
done:
    mbedtls_sha256_free(&ctx); fclose(f); return rc;
}
int main(int argc,char **argv)
{
    unsigned int running[3]; ProxCatalogOffer offer; int result;
    if(argc!=3) return 3;
    directory=argv[1]; mode=argv[2];
    if(sscanf(PROX_PRODUCT_VERSION,"%u.%u.%u",running,running+1,running+2)!=3) return 3;
    if(!strcmp(mode,"client-mismatch")) ++running[2];
    result=prox_catalog_query("fixture CA",running,1,&offer);
    printf("QUERY result=%d requests=%d pages=%u candidates=%u rejected=%u version=%s\n",
           result,requests,offer.pages,offer.candidates,offer.rejected,offer.manifest.version);
    return result==1?0:result==0?2:1;
}
