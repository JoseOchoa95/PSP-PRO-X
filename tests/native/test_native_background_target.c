/* Actual worker preparation with isolated decoder/crypto adapters and host files.
 * The real checked cryptographic/parser pipeline has its own separate suite. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "native_target_shim.h"
#include "native_background.h"
#include "native_signed.h"
static unsigned char original[40+0x150];
static int scenario,decodes,stages,stops;
int prox_stopping(void) { return scenario==3 && ++stops>3; }
int mbedtls_sha256(const unsigned char *p,size_t n,unsigned char *out,int mode)
{ assert(p && n && !mode);memset(out,0xab,32);return 0; } /* Adapter only. */
void prox_log(const char *fmt,...) { assert(fmt); }
int prox_stage_check(const char *root,const ProxManifest *m)
{ assert(root && m);++stages;return scenario==4 && stages==2?-1:0; }
int prox_file_check(const char *path,unsigned int n,const char *hash,int pbp)
{
    unsigned char data[2];FILE *f;size_t count;
    assert(!pbp && hash[0]=='a' && strstr(path,"/native/") && !strstr(path,"launcher"));
    f=fopen(path,"rb");assert(f);count=fread(data,1,sizeof(data),f);assert(!ferror(f) && !fclose(f));
    assert(n==1 && count==1 && data[0]<11);return scenario==2?-1:0;
}
int sceIoMkdir(const char *p,int mode) { return scenario==7?-1:mkdir(p,(unsigned int)mode); }
int sceIoRmdir(const char *p) { return rmdir(p); }
int kuKernelLoadModule(const char *p,int f,void *o)
{ (void)p;(void)f;(void)o;assert(!"No target module loader may run");return -1; }
int kuKernelLoadModuleWithApitype2(int api,const char *p,int f,void *o)
{ (void)api;(void)p;(void)f;(void)o;assert(!"No target API/device loader may run");return -1; }
int sceKernelQueryModuleInfo(int id,SceKernelModuleInfo *i) { (void)id;(void)i;assert(0);return -1; }
int sceKernelUnloadModule(int id) { (void)id;assert(0);return -1; }
int prox_native_signed_scan(ProxNativePayloadScan *s,const unsigned char *p,size_t n)
{
    unsigned int i;assert(n==0x150 && !memcmp(p,original+40,n));++decodes;if(scenario==1)return -1;
    for(i=0;i<11;++i) { unsigned char value=(unsigned char)i;if(s->sink(i,&value,1,s->context))return -1; }
    s->found=0x7ff;s->containers=3;return 0;
}
int prox_native_payload_done(const ProxNativePayloadScan *s) { assert(s->found==0x7ff && s->containers==3);return 0; }
int sceUtilityGetSystemParamInt(int i,int *l) { (void)i;(void)l;assert(0);return -1; }
int prox_completion_read_text(FILE *f,unsigned int l,unsigned short *o,unsigned int c)
{ (void)f;(void)l;(void)o;(void)c;assert(0);return -1; }
int prox_native_completion_prepare(const unsigned short *p,unsigned int n) { (void)p;(void)n;assert(0);return -1; }
int prox_native_handoff_close(void) { assert(0);return -1; }
int prox_native_install_exchange(int a,ProxNativeInstallMessage *m) { (void)a;(void)m;assert(0);return -1; }
int sceKernelStartModule(int i,unsigned int n,void *p,int *s,void *o)
{ (void)i;(void)n;(void)p;(void)s;(void)o;assert(!"No target code may start");return -1; }
int sceKernelDelayThread(unsigned int us) { (void)us;assert(0);return -1; }
static void word(unsigned int offset,unsigned int value)
{ unsigned int i;for(i=0;i<4;++i)original[offset+i]=(unsigned char)(value>>(8*i)); }
int main(void)
{
    ProxJob job={0};ProxManifest manifest={0};ProxNativeInstallJob result;char path[256];unsigned int i;
    strcpy(job.root,"/tmp/prox-background-target-fixture");mkdir(job.root,0700);
    snprintf(path,sizeof(path),"%s/PROUPDATE",job.root);mkdir(path,0700);
    strcpy(job.source_version,"0.3.2");strcpy(manifest.version,"0.3.3");strcpy(manifest.build_id,"pro-x-a30bdfa49f6e");
    manifest.files[0].size=sizeof(original);for(i=0;i<32;++i)strcpy(manifest.files[0].sha256+i*2,"ab");
    for(scenario=0;scenario<8;++scenario) {
        FILE *f;int rc;decodes=stages=stops=0;
        memset(original,0,sizeof(original));memcpy(original,"\0PBP",4);word(4,0x10000);
        for(i=0;i<8;++i)word(8+i*4,40);
        word(36,sizeof(original));memcpy(original+40,"~PSP",4);word(40+0x2c,0x150);
        manifest.files[0].sha256[0]=scenario==5?'f':'a';if(scenario==6)original[1]='X';
        snprintf(path,sizeof(path),"%s/PROUPDATE/EBOOT.PBP",job.root);f=fopen(path,"wb");assert(f);assert(fwrite(original,1,sizeof(original),f)==sizeof(original));assert(!fclose(f));
        rc=prox_native_background_prepare(&job,&manifest,1,&result);
        if(!scenario) { assert(!rc && decodes==1);for(i=0;i<11;++i)assert(result.parts[i].size==1);prox_native_background_discard(job.root); }
        else assert(rc<0);
        if(scenario>=5)assert(!decodes);
        snprintf(path,sizeof(path),"%s/native",job.root);assert(access(path,F_OK)!=0);
    }
    puts("PASS: actual worker pins PBP before data-only extraction; every target loader/start forbidden; partial output, cancel, revalidation, digest/header and storage errors clean up before flash");
    return 0;
}
