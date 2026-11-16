/* Actual resident worker and kernel-owned protocol. No device/flash/LoadExec. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "native_service_shim.h"
#include "native_service.h"
#include "native_query_state.h"
#include "native_download.h"
#include "stage.h"
#include "prox_product.h"
int prox_native_mode=1;
static ProxNativeQueryState state;
static jmp_buf finished;
static int mode,opened,queries,downloads,launches,removes,borrowed,live=1,report_open,steps,power_calls;
static unsigned int expected_running[3];
int proxNativeQueryService(int action,ProxNativeQueryMessage *m,unsigned int size)
{
    assert(size==sizeof(*m));
    return prox_nq_exchange(&state,2,action,m);
}
int sceIoGetstat(const char *p,SceIoStat *s) { (void)s; return mode>=9?-1:!strcmp(p,"ms0:/PSP/GAME/PROXUPDATE")?0:-1; }
int sceIoOpen(const char *p,int f,int m)
{
    assert(strstr(p,"ms0:/PSP/GAME/PROXUPDATE/native-") && f==(2|0x200|0x800) && m==0600 && !report_open);
    ++opened; if(mode>=9 && mode!=12) return -1; report_open=1; return 9;
}
int sceIoWrite(int fd,const void *p,unsigned int n) { assert(fd==9 && report_open && p); return mode==12?-1:(int)n; }
int prox_native_trust(void) { return 0; }
int sceIoClose(int fd) { assert(fd==9 && report_open); report_open=0; return 0; }
void prox_log(const char *f,...) { char b[384]; va_list v; va_start(v,f); vsnprintf(b,sizeof(b),f,v); va_end(v); prox_native_log_line(b); }
int prox_transport_new_query(void) { assert(!borrowed); return 0; }
int prox_network_borrow(void) { assert(!borrowed); borrowed=1; return 0; }
int prox_network_end(void) { assert(borrowed); borrowed=0; return 0; }
int proxUpdateGetState(int kind)
{ if(kind==0) return live; if(kind==4) return 0x06060110; if(kind==5) return 1; return (int)expected_running[kind-1]; }
int prox_power_ready(void) { ++power_calls; return mode!=4 && mode!=7 && mode!=10 && !(mode==8 && downloads); }
int prox_catalog_query(const char *ca,const unsigned int running[3],unsigned int model,ProxCatalogOffer *offer)
{
    assert(strstr(ca,"cacert.pem") && !memcmp(running,expected_running,sizeof(expected_running)) && model==1 && borrowed);
    ++queries; offer->pages=1; offer->manifest.package_size=131072;
    strcpy(offer->manifest.version,"0.3.1"); strcpy(offer->manifest.build_id,"pro-x-583ad26f7cce");
    memset(offer->manifest_sha256,'a',64); strcpy(offer->manifest.summary,"Exact authenticated offer fixture");
    return mode==11?-1:mode==5 || mode==7 || mode==10?0:1;
}
int prox_native_download_observed(const char *d,const char *ca,const unsigned int r[3],unsigned int model,
                                 const ProxCatalogOffer *offer,ProxJob *job,ProxNativeProgress progress,void *context)
{
    assert(!strcmp(d,"ms0:/PSP/GAME/PROXUPDATE") && strstr(ca,"cacert.pem") && model==1 && borrowed);
    assert(!memcmp(r,expected_running,sizeof(expected_running)) && !strcmp(offer->manifest.version,"0.3.1"));
    assert(offer->manifest_sha256[0]=='a' && !strcmp(offer->manifest.summary,"Exact authenticated offer fixture"));
    ++downloads; assert(!progress(32768,131072,context));
    if(mode==1) { prox_nq_cancel(&state); return -1; }
    if(mode==2) return -1;
    assert(!progress(131072,131072,context));
    strcpy(job->root,"ms0:/PSP/GAME/PROXUPDATE/.stage-pro-x-583ad26f7cce-00000001");
    strcpy(job->source_version,PROX_PRODUCT_VERSION); memset(job->manifest_sha256,'a',64);
    return 0;
}
void prox_stage_remove(const char *p) { assert(strstr(p,".stage-pro-x-583ad26f7cce")); ++removes; }
int prox_stage_check(const char *p,const ProxManifest *m)
{ assert(strstr(p,".stage-") && !strcmp(m->version,"0.3.1")); return mode==6?-1:0; }
int prox_launch_job(const ProxJob *job)
{
    assert(!borrowed && !report_open && !state.cancel && state.phase==PROX_NQ_HANDOFF);
    assert(!strcmp(job->source_version,PROX_PRODUCT_VERSION) && job->manifest_sha256[0]=='a'); ++launches;
    return -1; /* Failed loader must not report installation or reboot success. */
}
int sceKernelDelayThread(unsigned int us)
{
    assert(us==50000);
    assert(++steps<100); /* A broken lifecycle must fail instead of hanging CI. */
    if(state.command==PROX_NQ_IDLE && state.done && state.phase==PROX_NQ_QUERY) {
        assert(queries==1 && !borrowed && !report_open);
        if(mode==5 || mode==7 || mode==10 || mode==11) {
            assert(state.answer.result==(mode==11?-1:0) && !state.answer.failure && !power_calls);
            longjmp(finished,1);
        }
        /* A cardless positive query first returns its real offer. Storage is
         * required only after this explicit native download decision. */
        assert(state.answer.result==1 && !state.answer.failure && !power_calls);
        assert(!prox_nq_download(&state,1));
    } else if(state.phase==PROX_NQ_VERIFIED) {
        if(mode==3) prox_nq_cancel(&state);
        else assert(!prox_nq_handoff(&state,1));
    } else if(state.phase==PROX_NQ_FAILED) {
        assert(!borrowed && !report_open && !state.active && state.answer.result==-1);
        if(mode==4) assert(state.answer.failure==PROX_NQ_CAUSE_POWER);
        if(mode==8) assert(state.answer.failure==PROX_NQ_CAUSE_POWER);
        if(mode==6) assert(state.answer.failure==PROX_NQ_CAUSE_CLIENT);
        if(mode==2) assert(state.answer.failure==PROX_NQ_CAUSE_NONE);
        if(mode==9 || mode==12) assert(state.answer.failure==PROX_NQ_CAUSE_STORAGE && !downloads && !power_calls);
        longjmp(finished,1);
    }
    return 0;
}
int main(void)
{
    char *args[]={"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP","--prox-native-query-v3","0000004d"};
    sscanf(PROX_PRODUCT_VERSION,"%u.%u.%u",expected_running,expected_running+1,expected_running+2);
    for(mode=0;mode<13;++mode) {
        ProxNativeQueryMessage m={0}; opened=queries=downloads=launches=removes=borrowed=report_open=steps=power_calls=0;
        prox_nq_reset(&state,77); assert(!prox_nq_open(&state));
        m.protocol=PROX_NQ_PROTOCOL; m.lease=77;
        assert(!prox_nq_exchange(&state,2,PROX_NQ_REGISTER,&m)); assert(!prox_nq_submit(&state));
        if(!setjmp(finished)) prox_native_worker(3,args);
        assert(queries==1 && !borrowed && !report_open);
        if(mode==9 || mode==12) assert(opened==2 && !downloads && !launches && !power_calls);
        else if(mode==5 || mode==7 || mode==10 || mode==11) assert(opened==1 && !downloads && !launches && !power_calls);
        else { assert(opened==2); assert(downloads==(mode==4?0:1)); assert(launches==(mode==0?1:0)); }
        assert(removes==(mode==3?1:0));
    }
    puts("PASS: actual resident worker retains exact offer/generation, owns private stage, borrows/releases networking, cancels/cleans up and gates target LoadExec; returned launch never succeeds");
}
