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
int prox_native_test_valid(const void *p,unsigned int n) { return p && n && n<=sizeof(ProxNativeQueryMessage); }
int prox_preview_test_bytes(const void *p,unsigned int n) { return p && n && n<=32768; }
int pspSdkSetK1(int n) { (void)n; return 0; }
unsigned int pspSdkDisableInterrupts(void) { assert(!locked); locked=1; return 7; }
void pspSdkEnableInterrupts(unsigned int n) { assert(locked && n==7); locked=0; }
int sceKernelGetThreadId(void) { assert(!locked); return thread; }
unsigned int sceKernelGetSystemTimeLow(void) { assert(!locked); return ticks; }
void *sceKernelFindModuleByName(const char *n) { (void)n; return loaded?(void *)1:0; }
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
        m.result=-1; assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))); /* Returned launch fails. */
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
                assert(rc==32768 && ++i<=3 && last_progress>=(unsigned int)i*32768);
                assert(proxNativePreviewWrite(fd,bytes,rc-1)<0);
                assert(proxNativePreviewWrite(fd,bytes,rc)==rc);
            }
            assert(rc==(int)0x805f0007 && !detail);
            if(mode==0) assert(i==3 && staged && handed==1);
            else assert(!i && !staged && !handed);
        }
        if(mode==5) assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_download_cannot_connect"),"msgupdate_error_start"));
        if(mode==2) assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_download_cannot_connect"),"msgupdate_error_download_cannot_connect"));
        assert(!proxNativePreviewClose(fd)); assert(!end());
        /* Model the resident worker acknowledging cancellation before next InitEx. */
        if(mode==3) { thread=2; ProxNativeQueryMessage m={0}; m.protocol=PROX_NQ_PROTOCOL; m.lease=lease_id;
            assert(!proxNativeQueryService(PROX_NQ_POLL,&m,sizeof(m))); m.result=-1;
            assert(!proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))); thread=1; }
    }
    prox_native_query_load_failed_reason(PROX_NQ_CAUSE_STORAGE);
    assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_connect"),"msgshare_error_notplay_ms"));
    loaded=0; assert(!init((void *)1,(void *)2));
    assert(!strcmp(proxNativeQueryTextKey("msgupdate_error_connect"),"msgupdate_error_connect"));
    puts("PASS: native positive offer -> consent -> actual-byte progress -> verified-only handoff; cancel/error/current never launch; no native file/EOF/Sony fallback");
}
