/* Production provider + virtual I/O + stream; simulated owner RPC, no flash. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "prox_native_query.h"
#include "native_query_provider.h"
#include "native_offer_preview.h"
#include "native_error_text.h"
int proxNativeQueryService(int,ProxNativeQueryMessage *,unsigned int);
static int thread=1,locked,loaded,mode,started,staged,handed,steps;
static unsigned int ticks,lease_id,last_progress;
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
#include "prox_install_preview.h"
#include "prox_completion_preview.h"
static ProxCompletionState notice;
static unsigned short notice_text[120];
static int preview_loaded;
#ifdef PROX_NATIVE_BACKGROUND
#include "prox_product.h"
#include "prox_native_install.h"
int prox_native_install_test_module(void) { return 17; }
#endif
void prox_native_completion_reset(void) { memset(&notice,0,sizeof(notice)); }
int prox_native_completion_arm(unsigned int g,const unsigned short *p,unsigned int n)
{ assert(locked);return prox_completion_arm(&notice,g,notice_text,p,n); }
int prox_native_completion_ready(unsigned int g) { assert(locked);return g==notice.generation && notice.ready; }
int prox_native_completion_health(unsigned int g,unsigned int now) { assert(locked);return prox_completion_health(&notice,g,now); }
#endif
int prox_native_test_valid(const void *p,unsigned int n) {
#ifdef PROX_NATIVE_BACKGROUND
    return p && n && n<=sizeof(ProxNativeInstallMessage);
#else
    return p && n && n<=sizeof(ProxNativeQueryMessage);
#endif
}
int prox_preview_test_bytes(const void *p,unsigned int n) { return p && n && n<=32768; }
int pspSdkSetK1(int n) { (void)n; return 0; }
unsigned int pspSdkDisableInterrupts(void) { assert(!locked); locked=1; return 7; }
void pspSdkEnableInterrupts(unsigned int n) { assert(locked && n==7); locked=0; }
int sceKernelGetThreadId(void) { assert(!locked); return thread; }
unsigned int sceKernelGetSystemTimeLow(void) { assert(!locked); return ticks; }
void *sceKernelFindModuleByName(const char *n) {
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
    if(!strcmp(n,PROX_INSTALL_PREVIEW_MODULE)) return preview_loaded?(void *)1:0;
#endif
    (void)n;return loaded?(void *)1:0;
}
int prox_native_query_is_loading(void) { return 0; }
const char *prox_native_error_redirect(unsigned int failure,const char *key)
{ return prox_native_error_select(failure,key,"msgshare_error_notplay_ms","msgupdate_error_start"); }
void prox_native_query_event(const char *s,int r) { (void)s; (void)r; assert(!locked); }
int sceIoOpen(const char *p,int f,int m) { (void)p;(void)f;(void)m; assert(0); return -1; }
int sceIoWrite(int f,const void *p,int n) { (void)f;(void)p;(void)n; assert(0); return -1; }
int sceIoClose(int f) { (void)f; assert(0); return -1; }
int sceIoIoctl(int f,unsigned int c,void *i,int n,void *o,int z)
{ (void)f;(void)c;(void)i;(void)n;(void)o;(void)z; assert(0); return -1; }
int sceIoDevctl(const char *p,unsigned int c,void *i,int n,void *o,int z)
{ (void)p;(void)c;(void)i;(void)n;(void)o;(void)z; assert(0); return -1; }
int prox_native_query_start(unsigned int lease)
{
    ProxNativeQueryMessage m={0};
    if(loaded) return 0;
    loaded=1; lease_id=lease; m.protocol=PROX_NQ_PROTOCOL; m.lease=lease; thread=2;
    assert(!proxNativeQueryService(PROX_NQ_REGISTER,&m,sizeof(m))); thread=1; return 0;
}
int sceKernelDelayThread(unsigned int us)
{
    ProxNativeQueryMessage m={0}; assert(!locked); ticks+=us;
    m.protocol=PROX_NQ_PROTOCOL; m.lease=lease_id; thread=2;
    assert(!proxNativeQueryService(PROX_NQ_POLL,&m,sizeof(m)));
    if(m.command==PROX_NQ_CHECK) {
        m.result=mode==4?0:1; m.package_size=131072; strcpy(m.version,"0.3.1");
#ifdef PROX_NATIVE_BACKGROUND
        strcpy(m.version,"0.3.5"); /* Isolated future-target protocol fixture. */
#endif
        assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m)));
    } else if(m.command==PROX_NQ_DOWNLOAD) {
        started=1; ++steps;
        if(mode==1) { thread=1; ((int (*)(void))prox_native_query_function(0x4f49c9c1))(); thread=2; }
        if(mode==1 || mode==2 || mode==5) {
            if(mode==5) m.failure=PROX_NQ_CAUSE_POWER;
            m.result=-1; assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m)));
        } else {
            m.downloaded=last_progress+=32768;
            assert(!proxNativeQueryService(PROX_NQ_PROGRESS,&m,sizeof(m)));
            if(m.downloaded==131072) { assert(!proxNativeQueryService(PROX_NQ_STAGED,&m,sizeof(m))); staged=1; }
        }
    } else if(m.command==PROX_NQ_INSTALL) {
        assert(staged && last_progress==131072 && !m.cancel); ++handed;
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
        {
#ifdef PROX_NATIVE_BACKGROUND
            ProxNativeInstallMessage install={0};unsigned int part;
            install.protocol=1;install.lease=m.lease;install.generation=m.generation;install.module=17;
            strcpy(install.job.root,"ms0:/PSP/GAME/PROXUPDATE/.stage-pro-x-123456789abc-00112233");
            strcpy(install.job.build_id,"pro-x-123456789abc");strcpy(install.job.source_version,PROX_PRODUCT_VERSION);
            strcpy(install.job.target_version,"0.3.5");memset(install.job.manifest_sha256,'a',64);install.job.model=1;
            for(part=0;part<11;++part) { install.job.parts[part].size=512;memset(install.job.parts[part].sha256,'b',64); }
            thread=3;assert(proxNativeInstallService(PROX_NI_ARM,&install,sizeof(install))<0);thread=2;
            assert(!proxNativeInstallService(PROX_NI_ARM,&install,sizeof(install)));
            assert(!proxNativeInstallService(PROX_NI_BIND,&install,sizeof(install)));
            assert(proxNativeInstallService(PROX_NI_CLAIM,&install,sizeof(install))<0);
            thread=4;assert(!proxNativeInstallService(PROX_NI_CLAIM,&install,sizeof(install)));
            assert(!prox_native_install_completion_owner(4,m.generation));
            thread=2;assert(proxNativeInstallService(PROX_NI_WRITE,&install,sizeof(install))<0);
            thread=4;assert(!proxNativeInstallService(PROX_NI_WRITE,&install,sizeof(install)));
            thread=1;assert(!((int (*)(void))prox_native_query_function(0x4f49c9c1))());
            thread=4;assert(!proxNativeInstallService(PROX_NI_STATUS,&install,sizeof(install)) && !install.cancel);
            assert(!proxNativeInstallService(PROX_NI_COMMIT,&install,sizeof(install)));
            assert(prox_native_install_completion_owner(4,m.generation) && !prox_native_install_completion_owner(2,m.generation));
            thread=2;
#endif
            unsigned short text[]={'O','K',0};unsigned int saved=m.lease;
            m.downloaded=3;memcpy(m.summary,text,sizeof(text));
            thread=3;assert(proxNativeQueryService(PROX_NQ_NOTICE_ARM,&m,sizeof(m))<0);thread=2;
            m.lease^=1;assert(proxNativeQueryService(PROX_NQ_NOTICE_ARM,&m,sizeof(m))<0);m.lease=saved;
            --m.generation;assert(proxNativeQueryService(PROX_NQ_NOTICE_ARM,&m,sizeof(m))<0);++m.generation;
            m.downloaded=121;assert(proxNativeQueryService(PROX_NQ_NOTICE_ARM,&m,sizeof(m))<0);m.downloaded=3;
            assert(!proxNativeQueryService(PROX_NQ_NOTICE_ARM,&m,sizeof(m)));
            assert(!prox_completion_finish(&notice,m.generation,4));preview_loaded=1;
            notice.last_poll=ticks;
            thread=3;assert(proxNativeQueryService(PROX_NQ_NOTICE_STATUS,&m,sizeof(m))<0);thread=2;
            assert(!proxNativeQueryService(PROX_NQ_NOTICE_STATUS,&m,sizeof(m)) && m.result==1);
        }
#else
        m.result=-1; assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))); /* Returned launch fails. */
#endif
    }
    thread=1; return 0;
}
int main(void)
{
    int (*init)(const void *,const void *)=prox_native_query_function(0xf6690a9a);
    int (*connect)(unsigned int *,unsigned int *)=prox_native_query_function(0xfd675e8d);
    int (*read)(void *,unsigned int,unsigned int *)=prox_native_query_function(0xfa9aa797);
    int (*end)(void)=prox_native_query_function(0xd6a09757);
    unsigned char bytes[32768]; unsigned int size,detail; int fd,rc,i,run;
    const char *path="ms0:/PSP/GAME/_UPDATE12345678901234567890/EBOOT.PBP";
    for(run=0;run<6;++run) {
        mode=run; started=staged=handed=steps=0; last_progress=0;
        assert(!init((void *)1,(void *)2));
        rc=connect(&size,&detail);
        if(mode==4) { assert(rc==(int)0x805f0004 && !size && !started); assert(!end()); continue; }
        assert(!rc && size==131072 && !detail && !started);
        assert(proxNativePreviewOpen(path,0x202,0)<0 && !started);
        fd=proxNativePreviewOpen(path,0x602,0); assert(fd>0 && !started);
        thread=3; assert(read(bytes,sizeof(bytes),&detail)==(int)0x805f0007 && !started); thread=1;
        if(mode==3) { assert(!end()); rc=read(bytes,sizeof(bytes),&detail); assert(rc<0 && !started && !handed); }
        else {
            i=0;
            while((rc=read(bytes,sizeof(bytes),&detail))>0) {
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
                assert(rc==32768 && ++i<=4 && last_progress>=(unsigned int)i*32768);
#else
                assert(rc==32768 && ++i<=3 && last_progress>=(unsigned int)i*32768);
#endif
                assert(proxNativePreviewWrite(fd,bytes,rc-1)<0);
                assert(proxNativePreviewWrite(fd,bytes,rc)==rc);
            }
            assert(rc==(int)0x805f0007 && !detail);
            if(mode==0) {
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
                assert(i==4 && staged && handed==1 && notice.ready && !notice.shown);
                assert(prox_native_completion_generation()==notice.generation);
                assert(!end() && prox_native_completion_generation()==notice.generation);
#ifdef PROX_NATIVE_BACKGROUND
                {
                    ProxNativeInstallMessage install={0};install.protocol=1;install.lease=lease_id;install.generation=notice.generation;
                    thread=4;assert(!proxNativeInstallService(PROX_NI_FAIL,&install,sizeof(install)));thread=1;
                }
#endif
                preview_loaded=0;
#else
                assert(i==3 && staged && handed==1);
#endif
            }
            else assert(!i && !staged && !handed);
        }
        if(mode==5) assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_download_cannot_connect"),"msgupdate_error_start"));
        if(mode==2) assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_download_cannot_connect"),"msgupdate_error_download_cannot_connect"));
        assert(!proxNativePreviewClose(fd)); assert(!end());
        /* Model the resident worker acknowledging cancellation before next InitEx. */
        if(mode==3
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
           || mode==0
#endif
        ) { thread=2; ProxNativeQueryMessage m={0}; m.protocol=PROX_NQ_PROTOCOL; m.lease=lease_id;
            assert(!proxNativeQueryService(PROX_NQ_POLL,&m,sizeof(m))); m.result=-1;
            assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))); thread=1; }
    }
    prox_native_query_load_failed_reason(PROX_NQ_CAUSE_STORAGE);
    assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_connect"),"msgshare_error_notplay_ms"));
    loaded=0; assert(!init((void *)1,(void *)2));
    assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_connect"),"msgupdate_error_connect"));
    puts("PASS: native positive offer -> consent -> actual-byte progress -> verified-only handoff; cancel/error/current never launch; no native file/EOF/Sony fallback");
}
