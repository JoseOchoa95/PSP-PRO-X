/* Fixture transport around the actual production catalog selector. */
#include "catalog.h"
#include "prox_product.h"
#include "transport.h"
#include "native_dedicated_stage.h"
#include "dedicated_writer.h"
#include "pspkernel.h"
#include <mbedtls/sha256.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *directory,*mode;
static int requests;
static unsigned int running[3];
static int staged_progress;
static int fault_fd = -1;
int proxUpdateGetState(int field)
{
    if(field==0 || field==5) return 1;
    if(field==4) return 0x06060110;
    assert(field>=1 && field<=3);
    return (int)running[field-1];
}
int prox_power_ready(void) { return strcmp(mode,"dedicated-stage-power")!=0; }
int prox_storage_space(const char *path,unsigned long long needed)
{ assert(!strcmp(path,"ms0:/PROXDIAG") && needed>262144); return strcmp(mode,"dedicated-stage-space") ? 0 : -1; }
int sceIoMkdir(const char *path,int permissions) { return mkdir(path,permissions); }
int sceIoRmdir(const char *path) { return rmdir(path); }
int sceIoSync(const char *device, unsigned int flags) { (void)device; (void)flags; assert(0); return -1; }
int sceIoRemove(const char *path) { return unlink(path); }
int sceIoOpen(const char *path,int flags,int permissions)
{
    assert(flags==(PSP_O_WRONLY|PSP_O_CREAT|PSP_O_EXCL));
    int fd = open(path,O_WRONLY|O_CREAT|O_EXCL,permissions);
    if (strstr(path, "/native/02.prx"))
        fault_fd = fd;
    return fd;
}
int sceIoWrite(int fd,const void *bytes,unsigned int n)
{
    if (fd == fault_fd && !strcmp(mode, "dedicated-stage-short-write"))
        --n;
    int count = (int)write(fd,bytes,n);
    if (fd == fault_fd && !strcmp(mode, "dedicated-stage-bad-readback")) {
        unsigned char altered = ((const unsigned char *)bytes)[0] ^ 1;
        assert(lseek(fd, 0, SEEK_SET) == 0 && write(fd, &altered, 1) == 1);
    }
    return count;
}
int sceIoClose(int fd) { return close(fd); }
int sceKernelDelayThread(unsigned int wait) { (void)wait;return 0; }
static int stage_progress(unsigned int received,unsigned int total,void *unused)
{
    (void)unused;
    assert(received>(unsigned int)staged_progress && received<=total);
    staged_progress=(int)received;
    return !strcmp(mode,"dedicated-stage-cancel") ? -1 : 0;
}
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
    else if(strstr(url,"/pro-x-update.json") || strstr(url,"/pro-x-dedicated-update.json"))
        snprintf(name,sizeof(name),"%s/manifest.json",directory);
    else if(!strncmp(mode,"dedicated-stage",15) && strstr(url,"/EBOOT.PBP") && requests==4)
        snprintf(name,sizeof(name),"%s/package.pbp",directory);
    else return -1; /* No package request is allowed in a catalog-only mode. */
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
    ProxCatalogOffer offer; int result;
    if(argc!=3) return 3;
    directory=argv[1]; mode=argv[2];
    if(sscanf(PROX_PRODUCT_VERSION,"%u.%u.%u",running,running+1,running+2)!=3) return 3;
    if(!strcmp(mode,"client-mismatch")) ++running[2];
    if(!strncmp(mode,"dedicated",9))
        result=prox_catalog_query_format("fixture CA",running,1,PROX_RELEASE_DEDICATED,&offer);
    else
        result=prox_catalog_query("fixture CA",running,1,&offer);
    if(result==1 && !strncmp(mode,"dedicated-stage",15)) {
        assert(!chdir(directory));
        ProxDedicatedJob job;
        int status=prox_native_dedicated_stage("ms0:/PROXDIAG","fixture CA",running,1,&offer,0,1,1,stage_progress,&job);
        if (!status) {
            if (!strcmp(mode,"dedicated-stage-altered-handoff")) {
                FILE *part=fopen("ms0:/PROXDIAG/.dedicated-00000001-00000001/native/10.prx","wb");
                assert(part && fputc(0,part)!=EOF && !fclose(part));
            }
            assert(prox_dedicated_writer_check(&job,1,2)<0);
            status=prox_dedicated_writer_check(&job,1,1);
            printf("WRITER CHECK result=%d\n",status);
            assert(!prox_native_dedicated_discard(&job));
        }
        assert(access("ms0:/PROXDIAG/.dedicated-00000001-00000001",F_OK)!=0);
        printf("STAGE result=%d progressed=%d\n",status,staged_progress);
        result=status ? -1 : 1;
    }
    printf("QUERY result=%d requests=%d pages=%u candidates=%u rejected=%u version=%s\n",
           result,requests,offer.pages,offer.candidates,offer.rejected,offer.manifest.version);
    return result==1?0:result==0?2:1;
}
