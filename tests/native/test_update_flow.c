/* Offline test adapter for the production update orchestrator. Never linked
 * into a PSP build. No flash/installer interface is present. */
#include "update.h"
#include "transport.h"
#include "stage.h"
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

static const char *fixture,*mode;
static int requests,confirms,downloaded;
int prox_stopping(void) { return 0; }
void prox_log(const char *format,...)
{ va_list a; va_start(a,format); vprintf(format,a); putchar('\n'); va_end(a); }
int proxUpdateGetState(int s)
{
    static const int state[]={1,0,3,0,0x06060110,1};
    if (!strcmp(mode,"changed-state") && downloaded && s == 2) return 5;
    return s < 6 ? state[s] : -1;
}
int prox_confirm(const char *p)
{ (void)p; ++confirms; return strcmp(mode,confirms == 1 ? "cancel-download" : "cancel-install") != 0; }
int prox_storage_space(const char *p,unsigned long long n)
{ (void)p; return !n || !strcmp(mode,"no-space") ? -1 : 0; }
int prox_power_ready(void) { return strcmp(mode,"no-power") != 0; }
int prox_new_stage(char out[192],const char *directory,const char *build)
{
    char name[192]; (void)build;
    if (snprintf(name,sizeof(name),"%s/stage",directory) >= (int)sizeof(name) || mkdir(name,0700)) return -1;
    strcpy(out,name); return 0;
}
int prox_https(const char *url,const char *ca,size_t cap,ProxSink sink,void *arg,ProxResponse *r)
{
    char name[512]; FILE *f; unsigned char bytes[4096]; size_t n;
    mbedtls_sha256_context ctx; int rc=-1;
    (void)ca; ++requests; memset(r,0,sizeof(*r)); r->status=200;
    if (strstr(url,"api.github.com/")) {
        if (!strcmp(mode,"rate-limit")) { r->status=429; return 0; }
        if (!strcmp(mode,"api-error")) { r->status=404; return 0; }
        if (!strcmp(mode,"network-error")) return -1;
        snprintf(name,sizeof(name),"%s/catalog.json",fixture);
    } else if (strstr(url,"/pro-x-update.json")) snprintf(name,sizeof(name),"%s/manifest.json",fixture);
    else { snprintf(name,sizeof(name),"%s/package.zip",fixture); downloaded=1; }
    f=fopen(name,"rb"); if (!f) return -1;
    mbedtls_sha256_init(&ctx); mbedtls_sha256_starts(&ctx,0);
    while ((n=fread(bytes,1,sizeof(bytes),f)) != 0) {
        if (n > cap-r->bytes || sink(bytes,n,arg)) goto done;
        r->bytes+=n; mbedtls_sha256_update(&ctx,bytes,n);
    }
    if (ferror(f)) goto done;
    mbedtls_sha256_finish(&ctx,r->sha256);
    if (!strcmp(mode,"bad-package-hash") && downloaded) r->sha256[0]^=1;
    rc=0;
done:
    mbedtls_sha256_free(&ctx); fclose(f); return rc;
}
int main(int argc,char **argv)
{
    unsigned int running[3]={0,3,0}; ProxJob job; int rc;
    if (argc != 4) return 3;
    fixture=argv[1]; mode=argv[3];
    rc=prox_prepare_update(argv[2],"isolated fixture",running,1,&job);
    printf("FLOW result=%d requests=%d confirmations=%d root=%s\n",rc,requests,confirms,job.root);
    return rc == 1 ? 0 : rc == 0 ? 2 : 1;
}
