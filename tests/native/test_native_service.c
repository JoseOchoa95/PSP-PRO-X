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
#if defined(PROX_NATIVE_DEDICATED_PREVIEW) || defined(PROX_NATIVE_DEDICATED_OTA)
#define TEST_DEDICATED_SETTINGS
#include "native_dedicated_stage.h"
#include "dedicated_writer.h"
#include "prox_resident_query.h"
#endif

int prox_native_mode=1;
static ProxNativeQueryState state;
static jmp_buf finished;
static int mode,opened,queries,downloads,launches,removes,borrowed,live=1,report_open,steps,power_calls;
static unsigned int expected_running[3];
#ifdef PROX_NATIVE_BACKGROUND
#include "native_background.h"
#ifdef PROX_NATIVE_DEDICATED_OTA
#include "dedicated_job.h"
static int writer_starts, cleanup_case, cleanup_requests, cleanup_card, startup_cleanup;
int prox_dedicated_pending(const char *directory)
{
    assert(!borrowed);
    if(!strcmp(directory,PROX_RESIDENT_EF_REPORTS))return 0;
    assert(!strcmp(directory,PROX_RESIDENT_MS_REPORTS));
    return mode==5 && cleanup_case!=0;
}
int prox_dedicated_read_committed(const char *directory,const char *build,const char *version,
                                 unsigned int model,ProxDedicatedJob *job,ProxNativeFlash *transaction)
{
    assert(!strcmp(directory,PROX_RESIDENT_MS_REPORTS) && !strcmp(build,PROX_BUILD_ID));
    assert(!strcmp(version,PROX_PRODUCT_VERSION) && model==1);
    memset(job,0,sizeof(*job));
    memset(transaction,0,sizeof(*transaction));
    strcpy(job->install.build_id,build);
    return cleanup_case==2 ? -1 : 0;
}
int prox_dedicated_cleanup_committed(const char *directory,const char *build,const char *version,unsigned int model)
{
    assert(!strcmp(directory,PROX_RESIDENT_MS_REPORTS) && !strcmp(build,PROX_BUILD_ID));
    assert(!strcmp(version,PROX_PRODUCT_VERSION) && model==1 && cleanup_requests==1);
    ++cleanup_card;
    return 0;
}
int proxNativeInstallService(int action,ProxNativeInstallMessage *m,unsigned int size)
{
    const ProxNativeCleanupMessage *request=(const void *)m;
    assert(action==PROX_NI_CLEANUP_COMMITTED && size==sizeof(*request));
    assert(mode==5 && !borrowed && state.phase==PROX_NQ_QUERY && report_open);
    assert(state.command==(startup_cleanup ? PROX_NQ_BOOT_CLEANUP : PROX_NQ_CHECK));
    assert(request->protocol==PROX_NI_CLEANUP_PROTOCOL && request->lease==77 && request->generation==state.generation);
    assert(request->model==1 && !strcmp(request->build_id,PROX_BUILD_ID));
    ++cleanup_requests;
    return cleanup_case==3 ? -1 : 0;
}
#else
int proxNativeInstallService(int action,ProxNativeInstallMessage *m,unsigned int size)
{ (void)action;(void)m;(void)size;assert(0);return -1; }
#endif
#ifdef PROX_NATIVE_DEDICATED_OTA
int prox_native_background_launch_dedicated(const ProxDedicatedJob *job)
{
    assert(launches==1 && !borrowed && report_open && state.phase==PROX_NQ_HANDOFF);
    assert(!strcmp(job->install.root,"ms0:/PROXDIAG/.dedicated-0000004d-00000001"));
    assert(!prox_native_handoff_close());++writer_starts;return 0;
}
int prox_native_background_wait(void) { assert(writer_starts==1 && !report_open);return -1; }
#else
int prox_native_background_prepare(const ProxJob *job,const ProxManifest *manifest,unsigned int model,ProxNativeInstallJob *out)
{
    assert(!borrowed && report_open && model==1 && !strcmp(manifest->version,"0.3.1"));
    assert(!strcmp(job->source_version,PROX_PRODUCT_VERSION));
    memset(out,0,sizeof(*out));strcpy(out->root,job->root);return 0;
}
void prox_native_background_discard(const char *root)
{ assert(strstr(root,".stage-pro-x-583ad26f7cce")); }
int prox_native_background_launch(const char *directory,const ProxNativeInstallJob *job)
{
    assert(strstr(job->root,".stage-") && !strcmp(directory,"ms0:/PSP/GAME/PROXUPDATE"));
    assert(!borrowed && report_open && !state.cancel && state.phase==PROX_NQ_HANDOFF);
    assert(!prox_native_handoff_close() && !report_open);++launches;return 0;
}
int prox_native_background_wait(void) { assert(!report_open);return -1; }
#endif
#endif
int proxNativeQueryService(int action,ProxNativeQueryMessage *m,unsigned int size)
{
    assert(size==sizeof(*m));
    return prox_nq_exchange(&state,2,action,m);
}
int sceIoGetstat(const char *p,SceIoStat *s) { (void)s; return mode>=9?-1:!strcmp(p,"ms0:/PSP/GAME/PROXUPDATE")?0:-1; }
int sceIoOpen(const char *p,int f,int m)
{
#ifdef TEST_DEDICATED_SETTINGS
    assert(strstr(p,"ms0:/PROXDIAG/native-") && f==(2|0x200|0x800) && m==0600 && !report_open);
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(startup_cleanup)assert(strstr(p,"native-cleanup-"));
#endif
#else
    assert(strstr(p,"ms0:/PSP/GAME/PROXUPDATE/native-") && f==(2|0x200|0x800) && m==0600 && !report_open);
#endif
    ++opened; if(mode>=9 && mode!=12) return -1; report_open=1; return 9;
}
int sceIoWrite(int fd,const void *p,unsigned int n) { assert(fd==9 && report_open && p); return mode==12?-1:(int)n; }
int prox_native_trust(void) { return 0; }
int sceIoClose(int fd) { assert(fd==9 && report_open); report_open=0; return 0; }
void prox_log(const char *f,...) {
    char b[384]; va_list v; va_start(v,f); vsnprintf(b,sizeof(b),f,v); va_end(v);
#ifdef PROX_NATIVE_INSTALL_PREVIEW
    if(strstr(b,"NO-FLASH INSTALL PREVIEW: synthetic offer")) ++queries;
#endif
    prox_native_log_line(b);
}
int prox_transport_new_query(void) { assert(!borrowed); return 0; }
int prox_network_borrow(void) { assert(!borrowed); borrowed=1; return 0; }
int prox_network_end(void) { assert(borrowed); borrowed=0; return 0; }
int proxUpdateGetState(int kind)
{ if(kind==0) return live; if(kind==4) return 0x06060110; if(kind==5) return 1; return (int)expected_running[kind-1]; }
int prox_power_ready(void)
{
    ++power_calls;
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(mode==5 && cleanup_case==4)return 0;
#endif
    return mode!=4 && mode!=7 && mode!=10 && !(mode==8 && downloads);
}
#ifdef TEST_DEDICATED_SETTINGS
int prox_stopping(void) { return prox_native_is_cancelled(); }
int sceIoMkdir(const char *p,int mode_bits) { assert(!strcmp(p,PROX_RESIDENT_MS_REPORTS) && mode_bits==0777); return 0; }
int prox_catalog_query_format(const char *ca,const unsigned int running[3],unsigned int model,
                              unsigned int format,ProxCatalogOffer *offer)
{
    (void)ca;
    assert(!memcmp(running,expected_running,sizeof(expected_running)) && model==1 && borrowed);
    assert(format==PROX_RELEASE_DEDICATED);
    ++queries;
    offer->release.format=format;
    offer->manifest.package_size=131072;
    strcpy(offer->manifest.version,"0.5.0");
    return mode==5?0:1;
}
int prox_native_dedicated_stage(const char *directory,const char *ca,const unsigned int running[3],
                                unsigned int model,const ProxCatalogOffer *offer,int fixture,
                                unsigned int lease,unsigned int generation,ProxNativeProgress progress,
                                ProxDedicatedJob *out)
{
    (void)ca;
    assert(!strcmp(directory,PROX_RESIDENT_MS_REPORTS) && model==1 && borrowed && report_open);
    assert(!memcmp(running,expected_running,sizeof(expected_running)) && !fixture);
    assert(lease==77 && generation==state.generation && state.phase==PROX_NQ_TRANSFER);
    assert(offer->release.format==PROX_RELEASE_DEDICATED && !strcmp(offer->manifest.version,"0.5.0"));
    assert(prox_power_ready());
    ++downloads;
    assert(!progress(32768,131072,NULL));
    if(mode==1) { prox_nq_cancel(&state);return -1; }
    if(mode==2)return -1;
    assert(!progress(131072,131072,NULL));
    strcpy(out->install.root,"ms0:/PROXDIAG/.dedicated-0000004d-00000001");
    strcpy(out->install.source_version,PROX_PRODUCT_VERSION);
    return 0;
}
int prox_dedicated_writer_check(const ProxDedicatedJob *job,unsigned int lease,unsigned int generation)
{
    assert(!strcmp(job->install.root,"ms0:/PROXDIAG/.dedicated-0000004d-00000001"));
    assert(lease==77 && generation==state.generation && state.phase==PROX_NQ_HANDOFF);
    assert(!borrowed && !state.cancel && report_open && power_calls==3);
    ++launches;
    return mode==6?-1:0;
}
int prox_native_dedicated_discard(const ProxDedicatedJob *job)
{
    assert(!strcmp(job->install.root,"ms0:/PROXDIAG/.dedicated-0000004d-00000001"));
    assert(!borrowed && report_open);
    ++removes;
    return 0;
}
#endif
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
#ifdef PROX_NATIVE_INSTALL_PREVIEW
int prox_install_preview_download(const char *directory,ProxNativeProgress progress)
{
    assert(!strcmp(directory,"ms0:/PSP/GAME/PROXUPDATE") && !borrowed);
    /* Independent regression: the old preview branch reached this function
     * with zero power checks and must fail this fixture. */
    assert(power_calls>0 && mode!=4);
    ++downloads;
    assert(!progress(32768,1024u*1024u,NULL));
    if(mode==1) { prox_nq_cancel(&state); return -1; }
    if(mode==2) return -1;
    assert(!progress(1024u*1024u,1024u*1024u,NULL));
    return 0;
}
int prox_install_preview_launch(const char *directory)
{
    assert(!strcmp(directory,"ms0:/PSP/GAME/PROXUPDATE"));
    assert(!borrowed && !report_open && !state.cancel && state.phase==PROX_NQ_HANDOFF);
    ++launches; return -1;
}
#endif
int sceKernelDelayThread(unsigned int us)
{
    assert(us==50000);
    assert(++steps<100); /* A broken lifecycle must fail instead of hanging CI. */
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(startup_cleanup) {
        assert(!state.active && !state.done && !state.fault && state.command==PROX_NQ_IDLE);
        assert(state.ready && state.owner==2 && !state.answer.package_size && !queries && !borrowed && !report_open);
        longjmp(finished,1);
    }
#endif
    if(state.command==PROX_NQ_IDLE && state.done && state.phase==PROX_NQ_QUERY) {
        assert(queries==1 && !borrowed && !report_open);
        if(mode==5 || mode==7 || mode==10 || mode==11) {
            assert(state.answer.result==(mode==11?-1:0) && !state.answer.failure);
#ifdef PROX_NATIVE_DEDICATED_OTA
            assert(power_calls==(cleanup_case==1 || cleanup_case==3 || cleanup_case==4));
#else
            assert(!power_calls);
#endif
            longjmp(finished,1);
        }
        /* A cardless positive query first returns its real offer. Storage is
         * required only after this explicit native download decision. */
        assert(state.answer.result==1 && !state.answer.failure && !power_calls);
        assert(!prox_nq_download(&state,1));
    } else if(state.phase==PROX_NQ_VERIFIED) {
        if(mode==3) prox_nq_cancel(&state);
        else {
            assert(prox_nq_handoff(&state,4)<0);
            assert(!prox_nq_handoff(&state,1));
        }
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
#ifdef TEST_DEDICATED_SETTINGS
    char *args[]={PROX_RESIDENT_WORKER_ARG,PROX_RESIDENT_QUERY_ARG,"0000004d"};
#else
    char *args[]={"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP","--prox-native-query-v3","0000004d"};
#endif
    sscanf(PROX_PRODUCT_VERSION,"%u.%u.%u",expected_running,expected_running+1,expected_running+2);
    for(mode=0;mode<13;++mode) {
#ifdef TEST_DEDICATED_SETTINGS
        if(mode!=0 && mode!=1 && mode!=2 && mode!=3 && mode!=4 && mode!=6 && mode!=8
#ifdef PROX_NATIVE_DEDICATED_OTA
           && mode!=5
#endif
          )continue;
#endif
#ifdef PROX_NATIVE_DEDICATED_OTA
        writer_starts=0;
#endif
#ifdef PROX_NATIVE_INSTALL_PREVIEW
        if(mode!=0 && mode!=1 && mode!=2 && mode!=3 && mode!=4 && mode!=8) continue;
#endif
        ProxNativeQueryMessage m={0}; opened=queries=downloads=launches=removes=borrowed=report_open=steps=power_calls=0;
        prox_nq_reset(&state,77); assert(!prox_nq_open(&state));
        m.protocol=PROX_NQ_PROTOCOL; m.lease=77;
        assert(!prox_nq_exchange(&state,2,PROX_NQ_REGISTER,&m)); assert(!prox_nq_submit(&state));
        if(!setjmp(finished)) prox_native_worker(3,args);
        assert(queries==1 && !borrowed && !report_open);
        if(mode==9 || mode==12) assert(opened==2 && !downloads && !launches && !power_calls);
        else if(mode==5 || mode==7 || mode==10 || mode==11) assert(opened==1 && !downloads && !launches && !power_calls);
        else {
            assert(opened==2); assert(downloads==(mode==4?0:1));
#ifdef TEST_DEDICATED_SETTINGS
            assert(launches==(mode==0 || mode==6));
#else
            assert(launches==(mode==0?1:0));
#endif
        }
#ifdef PROX_NATIVE_INSTALL_PREVIEW
        assert(!removes); /* No real stage exists in the preview. */
#elif defined(PROX_NATIVE_DEDICATED_OTA)
        assert(writer_starts==(mode==0) && removes==(mode==3 || mode==6 || mode==8));
#elif defined(PROX_NATIVE_DEDICATED_PREVIEW)
        assert(removes==(mode==0 || mode==3 || mode==6 || mode==8));
#else
        assert(removes==(mode==3?1:0));
#endif
    }
#ifdef PROX_NATIVE_INSTALL_PREVIEW
    puts("PASS: actual preview worker rejects missing AC before any progress/download, retains late power recheck, cancellation and returned-launch failure; no stage/network/flash");
#elif defined(PROX_NATIVE_DEDICATED_OTA)
    mode=5;
    for(cleanup_case=1;cleanup_case<=4;++cleanup_case) {
        ProxNativeQueryMessage m={0};
        opened=queries=downloads=launches=removes=borrowed=report_open=steps=power_calls=0;
        cleanup_requests=cleanup_card=0;
        prox_nq_reset(&state,77);
        assert(!prox_nq_open(&state));
        m.protocol=PROX_NQ_PROTOCOL;
        m.lease=77;
        assert(!prox_nq_exchange(&state,2,PROX_NQ_REGISTER,&m));
        assert(!prox_nq_submit(&state));
        if(!setjmp(finished)) prox_native_worker(3,args);
        assert(queries==1 && !borrowed && !report_open && !downloads && !launches);
        assert(cleanup_requests==(cleanup_case==1 || cleanup_case==3) && cleanup_card==(cleanup_case==1));
        assert(power_calls==(cleanup_case!=2));
    }
    puts("PASS: authenticated resident query retires card stage only after exact committed record, power readiness and kernel cleanup");
    startup_cleanup=1;
    for(cleanup_case=0;cleanup_case<=4;++cleanup_case) {
        opened=queries=downloads=launches=removes=borrowed=report_open=steps=power_calls=0;
        cleanup_requests=cleanup_card=0;
        prox_nq_reset(&state,77);
        assert(!prox_nq_start_boot_cleanup(&state));
        assert(prox_nq_start_boot_cleanup(&state)<0 && prox_nq_open(&state)<0);
        if(!setjmp(finished)) prox_native_worker(3,args);
        assert(!queries && !borrowed && !report_open && !downloads && !launches);
        assert(opened==(cleanup_case!=0) && power_calls==(cleanup_case!=0 && cleanup_case!=2));
        assert(cleanup_requests==(cleanup_case==1 || cleanup_case==3) && cleanup_card==(cleanup_case==1));
        assert(!prox_nq_open(&state) && !prox_nq_submit(&state));
    }
    puts("PASS: startup worker performs the same verified cleanup offline, leaves deferred work intact and returns ownership to normal System Update");
    puts("PASS: writable Settings worker requires actual offer and exact resident writer handoff, retains writer-owned stage and releases borrowed networking");
#elif defined(PROX_NATIVE_DEDICATED_PREVIEW)
    puts("PASS: dedicated resident worker holds the stage until the native consumer handoff, rechecks readiness, rejects failed writer input and cleans its stage on cancellation or return");
#else
    puts("PASS: actual resident worker retains exact offer/generation, owns private stage, borrows/releases networking, cancels/cleans up and gates target LoadExec; returned launch never succeeds");
#endif
}
