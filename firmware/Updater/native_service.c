/* Resident user-mode update worker. No renderer or HOME callback.
 * Own DNS/socket resources only; borrowed XMB networking is never terminated.
 * GPL-3.0-or-later. */
#ifdef PROX_NATIVE_SERVICE_TEST
#include "native_service_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#ifdef PROX_NATIVE_DEDICATED_OTA
#include <psputility.h>
#endif
#endif
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "native_service.h"
#include "prox_native_query.h"
#include "prox_product.h"
#include "prox_dedicated_storage.h"
#include "bridge.h"
#include "transport.h"
#include "network_session.h"
#include "catalog.h"
#include "native_download.h"
#include "stage.h"
#ifdef PROX_NATIVE_RESIDENT_QUERY
#include "prox_resident_query.h"
#endif
#if defined(PROX_NATIVE_DEDICATED_PREVIEW) || defined(PROX_NATIVE_DEDICATED_OTA)
#define PROX_NATIVE_DEDICATED_SETTINGS
#include "dedicated_update.h"
#ifdef PROX_NATIVE_DEDICATED_PREVIEW
#include "dedicated_fixture.h"
#endif
#include "dedicated_writer.h"
#include "native_dedicated_stage.h"
#include "policy.h"
static int fixture_offer, have_dedicated_offer;
static ProxDedicatedJob dedicated_job;
static int have_dedicated_stage;
#endif
#ifdef PROX_NATIVE_BACKGROUND
#include "native_background.h"
#ifndef PROX_NATIVE_RESIDENT_QUERY
static ProxNativeInstallJob native_job;
static int have_native;
#endif
#endif
#ifdef PROX_NATIVE_INSTALL_PREVIEW
#include "prox_install_preview.h"
#include "native_install_preview.h"
#endif
const char prox_native_service_marker[]="PROX-NATIVE-QUERY-V3";
static unsigned int lease_id,generation;
static int report_fd=-1,report_failed,report_required;
static char ca_path[100];
static ProxCatalogOffer retained;
#ifndef PROX_NATIVE_RESIDENT_QUERY
static char directory[80];
static ProxJob job;
static unsigned int offer_generation;
static int have_offer,have_stage;
#endif
static unsigned int running[3],model;
static unsigned int failure;
static void packet(ProxNativeQueryMessage *m)
{ memset(m,0,sizeof(*m)); m->protocol=PROX_NQ_PROTOCOL; m->lease=lease_id; m->generation=generation; }
int prox_native_install_exchange(int action,ProxNativeInstallMessage *message)
{
#ifdef PROX_NATIVE_BACKGROUND
    message->protocol=PROX_NI_PROTOCOL;message->lease=lease_id;message->generation=generation;
    return proxNativeInstallService(action,message,sizeof(*message));
#else
    (void)action;(void)message;return -1;
#endif
}
int prox_native_completion_prepare(const unsigned short *text,unsigned int count)
{
    ProxNativeQueryMessage m;
    if(!prox_native_mode || !text || count<2 || count>120) return -1;
    packet(&m);m.downloaded=count;memcpy(m.summary,text,count*sizeof(*text));
    return proxNativeQueryService(PROX_NQ_NOTICE_ARM,&m,sizeof(m));
}
void prox_native_log_line(const char *line)
{
    unsigned int length=(unsigned int)strlen(line);
    if(report_fd<0 || sceIoWrite(report_fd,line,length)!=(int)length || sceIoWrite(report_fd,"\n",1)!=1)
        report_failed=1;
}
int prox_native_completion_pending(void)
{
    ProxNativeQueryMessage m;
    if(!prox_native_mode) return -1;
    packet(&m);
    return proxNativeQueryService(PROX_NQ_NOTICE_STATUS,&m,sizeof(m))<0?-1:m.result;
}
int prox_native_is_cancelled(void)
{
    ProxNativeQueryMessage m;
    if(report_required && report_failed) return 1;
    packet(&m);
    return proxNativeQueryService(PROX_NQ_STATUS,&m,sizeof(m))<0 || m.cancel || m.generation!=generation;
}
void prox_native_resident_fault(void)
{
    ProxNativeQueryMessage m;
    if(!prox_native_mode) return;
    packet(&m); proxNativeQueryService(PROX_NQ_FAULT,&m,sizeof(m));
    if(report_fd>=0) { sceIoClose(report_fd); report_fd=-1; }
    /* A live resolver still owns code/static/stack pointers. Preserve them and
     * let the original UI show failure; never exit VSH or unload that code. */
    for(;;) sceKernelDelayThread(1000000);
}
static int open_report_at(const char *directory,const char *kind)
{
    char path[128]; unsigned int i; SceIoStat stat;
    report_failed=0;
    if(!directory)return -1;
    for(i=1;i<=999;++i) {
        snprintf(path,sizeof(path),"%s/native-%s-%03u.txt",directory,kind,i);
        if(sceIoGetstat(path,&stat)>=0) continue;
        report_fd=sceIoOpen(path,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_EXCL,0600);
        return report_fd<0?-1:0;
    }
    return -1;
}
#ifndef PROX_NATIVE_DEDICATED_OTA
static int open_report(const char *kind)
{
#ifdef PROX_NATIVE_RESIDENT_QUERY
    return open_report_at(prox_resident_report_directory(),kind);
#else
    return open_report_at(directory,kind);
#endif
}
#endif
#ifdef PROX_NATIVE_DEDICATED_OTA
static unsigned int write_storage_offer(char *destination, unsigned int package_size)
{
    /* Use the installed PSP language and keep the original page and controls. */
    static const unsigned short *const messages[] = {
        u"アップデートがあります。\nダウンロードにはメモリースティックに # MB 以上の空き容量が必要です。",
        u"An update is available.\nTo download it, free at least # MB on the Memory Stick.",
        u"Une mise à jour est disponible.\nPour la télécharger, libérez au moins # Mo sur le Memory Stick.",
        u"Hay una actualización disponible.\nPara descargarla, deja al menos # MB libres en la Memory Stick.",
        u"Ein Update ist verfügbar.\nFür den Download sind mindestens # MB auf dem Memory Stick erforderlich.",
        u"È disponibile un aggiornamento.\nPer scaricarlo, libera almeno # MB sulla Memory Stick.",
        u"Er is een update beschikbaar.\nMaak minstens # MB vrij op de Memory Stick om te downloaden.",
        u"Há uma atualização disponível.\nPara baixá-la, libere pelo menos # MB no Memory Stick.",
        u"Доступно обновление.\nДля загрузки требуется не менее # МБ на карте Memory Stick.",
        u"업데이트가 있습니다.\n다운로드하려면 Memory Stick에 # MB 이상을 확보하세요.",
        u"有可用更新。\n下載前請在 Memory Stick 保留至少 # MB 可用空間。",
        u"有可用更新。\n下载前请在 Memory Stick 上保留至少 # MB 可用空间。",
    };
    unsigned short text[PROX_OFFER_TEXT_UNITS];
    char number[12];
    int language;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &language) < 0 ||
        language < 0 || language >= (int)(sizeof(messages) / sizeof(messages[0])))
        language = 1;
    snprintf(number, sizeof(number), "%u", prox_dedicated_card_mb(package_size));
    unsigned int used = 0;
    for (const unsigned short *cursor = messages[language]; *cursor; ++cursor) {
        if (*cursor == '#') {
            for (const char *digit = number; *digit && used < PROX_OFFER_TEXT_UNITS-1; ++digit)
                text[used++] = (unsigned short)*digit;
        } else if (used < PROX_OFFER_TEXT_UNITS-1) {
            text[used++] = *cursor;
        }
    }
    text[used++] = 0;
    memcpy(destination, text, used * sizeof(*text));
    return used;
}

static void cleanup_verified_boot(void)
{
    static const char *const phases[]={
        "rpc-entry", "session", "worker-owner", "query-state", "kernel-busy",
        "target-identity", "worker-identity", "installed-module", "installed-worker",
        "temporary-module", "module-backup", "temporary-worker", "flash-mount", "flash-restore",
        "kernel-stack", "kernel-thread"
    };
    static const char *const checks[]={
        "none", "record", "stat", "type-or-size", "open", "read", "eof", "sha256", "close", "remove",
        "create-thread", "start-thread", "wait-thread"
    };
    const char *directory=PROX_RESIDENT_MS_REPORTS;
    int pending=prox_dedicated_pending(directory);
    if(pending<=0) {
        int internal=prox_dedicated_pending(PROX_RESIDENT_EF_REPORTS);
        if(internal>0) {
            directory=PROX_RESIDENT_EF_REPORTS;
            pending=internal;
        }
    }
    if(!pending)return;
    report_required=0;
    (void)open_report_at(directory,"cleanup");
    prox_log("PRO-X v" PROX_PRODUCT_VERSION " cleanup build " PROX_BUILD_ID);
    ProxDedicatedJob job;
    ProxNativeFlash transaction;
    unsigned char record_digest[32];
    const unsigned int model=(unsigned int)proxUpdateGetState(5);
    if(proxUpdateGetState(0)!=1 || proxUpdateGetState(4)!=0x06060110 ||
       model!=1 || proxUpdateGetState(1)!=PROX_PRODUCT_MAJOR ||
       proxUpdateGetState(2)!=PROX_PRODUCT_MINOR || proxUpdateGetState(3)!=PROX_PRODUCT_PATCH) {
        prox_log("Dedicated boot cleanup deferred: active target did not verify");
        goto done;
    }
    if(pending<0 || prox_dedicated_read_committed(directory,PROX_BUILD_ID,PROX_PRODUCT_VERSION,model,
                                                 &job,&transaction,record_digest)) {
        prox_log("Dedicated boot cleanup deferred: committed card record did not verify");
        goto done;
    }
    if(!prox_power_ready()) {
        prox_log("Dedicated boot cleanup deferred: installation power prerequisite");
        goto done;
    }
    ProxNativeCleanupMessage request={.protocol=PROX_NI_CLEANUP_PROTOCOL,.lease=lease_id,
        .generation=generation,.model=model,.prepared=transaction.prepared,.backups=transaction.backups,
        .worker_prepared=(unsigned int)transaction.worker_prepared,.worker=job.worker,
        .diagnostic={.phase=PROX_CLEANUP_ENTRY,.part=-1}};
    memcpy(request.build_id,job.install.build_id,sizeof(request.build_id));
    memcpy(request.installed,job.install.parts,sizeof(request.installed));
    memcpy(request.before,transaction.before,sizeof(request.before));
    if(prox_native_is_cancelled()) {
        prox_log("Dedicated boot cleanup deferred: worker session cancelled or unavailable");
        goto done;
    }
    int result=proxNativeInstallService(PROX_NI_CLEANUP_COMMITTED,(void *)&request,sizeof(request));
    if(result) {
        const ProxNativeCleanupDiagnostic *d=&request.diagnostic;
        const char *phase=(unsigned)d->phase<sizeof(phases)/sizeof(*phases)?phases[d->phase]:"unknown";
        const char *check=(unsigned)d->check<sizeof(checks)/sizeof(*checks)?checks[d->check]:"unknown";
        prox_log("Dedicated boot cleanup deferred: phase=%s part=%d check=%s detail=0x%08x rpc=%d",
                 phase,d->part,check,(unsigned int)d->detail,result);
        goto done;
    }
    /* The card finalizer removes the reports, including the current one. */
    if(report_fd>=0) {
        sceIoClose(report_fd);
        report_fd=-1;
    }
    result=prox_dedicated_cleanup_committed(directory,PROX_BUILD_ID,PROX_PRODUCT_VERSION,model,record_digest);
    if(result) {
        (void)open_report_at(directory,"cleanup");
        prox_log("Dedicated boot cleanup: deferred (%d)",result);
    }
done:
    if(report_fd>=0) {
        sceIoClose(report_fd);
        report_fd=-1;
    }
}
#endif
static int query(ProxCatalogOffer *offer)
{
    int i,result=-1,borrowed=0; time_t now;
#if defined(PROX_NATIVE_UI_QUERY) && !defined(PROX_NATIVE_OTA) && !defined(PROX_NATIVE_OFFER_PREVIEW)
    const unsigned int profile_start = sceKernelGetSystemTimeLow();
    const unsigned int free_before = sceKernelTotalFreeMemSize();
    const unsigned int largest_before = sceKernelMaxFreeMemSize();
#endif
    failure=PROX_NQ_CAUSE_NONE;
    /* Checking is read-only and uses resident code/trust. An optional trace
     * cannot make absence of download storage a query prerequisite. */
    report_required=0;
#ifndef PROX_NATIVE_DEDICATED_OTA
    (void)open_report("report");
#endif
    prox_log("PRO-X v" PROX_PRODUCT_VERSION " native REAL query build " PROX_BUILD_ID);
    prox_log("%s; original XMB UI; checking does not authorize package download",prox_native_service_marker);
#ifdef PROX_NATIVE_RESIDENT_QUERY
    prox_log("Resident query worker source: flash0; removable storage is optional for reports");
#endif
    if(proxUpdateGetState(0)!=1 || proxUpdateGetState(4)!=0x06060110) goto done;
    for(i=0;i<3;++i) running[i]=(unsigned int)proxUpdateGetState(i+1);
    prox_log("Running product=%u.%u.%u model_id=%d",running[0],running[1],running[2],proxUpdateGetState(5));
    now=time(NULL); prox_log("RTC epoch=%ld",(long)now);
    if(now<1735689600) { prox_log("Correct PSP date/time before authenticated HTTPS"); goto done; }
#ifdef PROX_NATIVE_INSTALL_PREVIEW
    prox_log("NO-FLASH INSTALL PREVIEW: synthetic offer, no GitHub query, no downloaded update");
    strcpy(offer->manifest.version,PROX_PRODUCT_VERSION);
    strcpy(offer->manifest.summary,"NO-FLASH background lifecycle diagnostic; not an available update");
    offer->manifest.package_size=PROX_INSTALL_PREVIEW_BYTES;
    result=1; goto done;
#endif
    if(prox_transport_new_query()) goto done;
    if(prox_network_borrow()) goto done;
    borrowed=1;
    model=(unsigned int)proxUpdateGetState(5);
#ifdef PROX_NATIVE_DEDICATED_SETTINGS
    result=prox_catalog_query_format(ca_path,running,model,PROX_RELEASE_DEDICATED,offer);
#ifdef PROX_NATIVE_DEDICATED_PREVIEW
    fixture_offer=0;
    if(!result && model==1 && !prox_stopping()) {
        offer->release.format=PROX_RELEASE_DEDICATED;
        strcpy(offer->manifest.version, PROX_FIXTURE_VERSION);
        strcpy(offer->manifest.build_id, PROX_FIXTURE_BUILD);
        strcpy(offer->manifest.package_sha256, PROX_FIXTURE_SHA);
        strcpy(offer->manifest_sha256, PROX_FIXTURE_MANIFEST_SHA);
        strcpy(offer->manifest.summary, "Dedicated EBOOT diagnostic; synthetic data, no installation");
        offer->manifest.package_size=PROX_FIXTURE_SIZE;
        offer->manifest.minimum_engine_revision=PROX_DEDICATED_ENGINE_REVISION;
        if(!prox_version(PROX_FIXTURE_VERSION,offer->manifest.numbers) &&
           prox_compare(offer->manifest.numbers,running)>0) {
            prox_log("DEDICATED PREVIEW: pinned synthetic offer after validated catalog");
            fixture_offer=1;result=1;
        }
    }
#endif
#else
    result=prox_catalog_query(ca_path,running,model,offer);
#endif
    if(result==1) prox_log("Authenticated compatible target %s retained for this generation; waiting for native consent",offer->manifest.version);
done:
    if(borrowed && prox_network_end()) result=-1;
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(result>=0 && !prox_native_is_cancelled())cleanup_verified_boot();
#endif
    if(prox_native_is_cancelled()) result=-1;
    prox_log("REAL native query completed result=%d pages=%u candidates=%u cause=%u",result,offer->pages,offer->candidates,failure);
#if defined(PROX_NATIVE_UI_QUERY) && !defined(PROX_NATIVE_OTA) && !defined(PROX_NATIVE_OFFER_PREVIEW)
    prox_log("Profile query elapsed_us=%u user_free_before=%u user_free_after=%u user_largest_before=%u user_largest_after=%u",
        sceKernelGetSystemTimeLow() - profile_start, free_before, sceKernelTotalFreeMemSize(),
        largest_before, sceKernelMaxFreeMemSize());
#endif
    if(report_fd>=0) sceIoClose(report_fd);
    report_fd=-1; return result;
}
#if !defined(PROX_NATIVE_RESIDENT_QUERY) || defined(PROX_NATIVE_DEDICATED_SETTINGS)
static int progress(unsigned int received,unsigned int total,void *context)
{
    ProxNativeQueryMessage m;
    (void)context;
    if(total!=retained.manifest.package_size || prox_native_is_cancelled()) return -1;
    packet(&m); m.downloaded=received;
    return proxNativeQueryService(PROX_NQ_PROGRESS,&m,sizeof(m));
}
#endif
#ifdef PROX_NATIVE_DEDICATED_SETTINGS
static int dedicated_transfer(void)
{
    int result=-1,borrowed=0;
    failure=PROX_NQ_CAUSE_NONE;
    report_required=1;
    const char *storage = prox_resident_report_directory();
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(storage && prox_dedicated_reset_download(storage)) {
        failure=PROX_NQ_CAUSE_CLIENT;
        return -1;
    }
#endif
    if(open_report_at(storage,"download")) {
        failure=PROX_NQ_CAUSE_STORAGE;
        return -1;
    }
    prox_log("PRO-X v" PROX_PRODUCT_VERSION " dedicated EBOOT build " PROX_BUILD_ID);
    if(!prox_power_ready()) {
        failure=PROX_NQ_CAUSE_POWER;
        goto done;
    }
    if(!fixture_offer) {
        if(prox_transport_new_query() || prox_network_borrow())
            goto done;
        borrowed=1;
    }
    result=prox_native_dedicated_stage(storage,ca_path,running,model,
                                       &retained,fixture_offer,lease_id,generation,progress,&dedicated_job);
    have_dedicated_stage=!result;
    if(result==PROX_DEDICATED_STAGE_STORAGE)failure=PROX_NQ_CAUSE_STORAGE;
    if(result==PROX_DEDICATED_STAGE_POWER)failure=PROX_NQ_CAUSE_POWER;
done:
    if(borrowed && prox_network_end())result=-1;
    if(report_failed || prox_native_is_cancelled())result=-1;
    prox_log("Dedicated transfer result=%d",result);
    return result;
}

static int dedicated_handoff(void)
{
    if(!have_dedicated_stage || prox_native_is_cancelled())return -1;
    if(!prox_power_ready()) {
        failure=PROX_NQ_CAUSE_POWER;
        return -1;
    }
    if(proxUpdateGetState(0)!=1 || proxUpdateGetState(4)!=0x06060110 ||
       proxUpdateGetState(5)!=(int)model || strcmp(dedicated_job.install.source_version,PROX_PRODUCT_VERSION))
        return -1;
    for(unsigned int i=0;i<3;++i) {
        if(proxUpdateGetState(i+1)!=(int)running[i])return -1;
    }
    int result=prox_dedicated_writer_check(&dedicated_job,lease_id,generation);
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(result || prox_native_background_launch_dedicated(&dedicated_job))return -1;
    have_dedicated_stage=0; /* The writer has claimed the stage for recovery. */
    return prox_native_background_wait();
#else
    prox_log("Dedicated Settings writer handoff: %s; read-only diagnostic",result?"FAIL":"PASS");
    return result;
#endif
}

static void dedicated_finish(void)
{
    if(have_dedicated_stage && prox_native_dedicated_discard(&dedicated_job))
        failure=PROX_NQ_CAUSE_STORAGE;
    have_dedicated_stage=0;
    have_dedicated_offer=0;
    memset(&dedicated_job,0,sizeof(dedicated_job));
    if(report_fd>=0) sceIoClose(report_fd);
    report_fd=-1;
    ProxNativeQueryMessage m;
    packet(&m);m.result=-1;m.failure=failure;
    if(proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))<0)
        prox_native_resident_fault();
}
#endif
#ifndef PROX_NATIVE_RESIDENT_QUERY
static void discard_stage(void)
{
#ifdef PROX_NATIVE_BACKGROUND
    if(have_native)prox_native_background_discard(job.root);
    have_native=0;memset(&native_job,0,sizeof(native_job));
#endif
#ifndef PROX_NATIVE_INSTALL_PREVIEW
    if(have_stage) prox_stage_remove(job.root);
#endif
    memset(&job,0,sizeof(job)); have_stage=0;
}
static int download(void)
{
    int result=-1,borrowed=0;
    failure=PROX_NQ_CAUSE_NONE;
    report_required=1;
    if(open_report("download")) { failure=PROX_NQ_CAUSE_STORAGE; return -1; }
    prox_log("PRO-X v" PROX_PRODUCT_VERSION " native REAL download build " PROX_BUILD_ID);
    prox_log("Consented generation=%u target=%s build=%s package=%u; verified installation handoff",
             generation,retained.manifest.version,retained.manifest.build_id,retained.manifest.package_size);
    if(report_failed) { failure=PROX_NQ_CAUSE_STORAGE; goto done; }
    if(!have_offer || offer_generation!=generation) goto done;
    /* The explicit diagnostic must obey the same pre-progress readiness
     * boundary as the real downloader. Recheck again before handoff. */
    if(!prox_power_ready()) { failure=PROX_NQ_CAUSE_POWER; goto done; }
#ifdef PROX_NATIVE_INSTALL_PREVIEW
    result=prox_install_preview_download(directory,progress);
    if(!result) have_stage=1;
    goto done;
#endif
    if(prox_native_is_cancelled() ||
       time(NULL)<1735689600 || prox_transport_new_query() || prox_network_borrow()) goto done;
    borrowed=1;
    result=prox_native_download_observed(directory,ca_path,running,model,&retained,&job,progress,NULL);
    if(!result) have_stage=1;
done:
    if(borrowed && prox_network_end()) result=-1;
#ifdef PROX_NATIVE_BACKGROUND
    if(!result) {
        if(prox_native_background_prepare(&job,&retained.manifest,model,&native_job)) {
            result=-1;failure=PROX_NQ_CAUSE_CLIENT;
        } else have_native=1;
    }
#endif
    if(report_failed || prox_native_is_cancelled()) result=-1;
    if(result && failure==PROX_NQ_CAUSE_NONE) {
        SceIoStat stat;
        if(report_failed || sceIoGetstat(directory,&stat)<0) failure=PROX_NQ_CAUSE_STORAGE;
        else if(!prox_power_ready()) failure=PROX_NQ_CAUSE_POWER;
    }
    if(result) discard_stage();
    prox_log("REAL native private download/stage result=%d cause=%u",result,failure);
    if(report_failed) { discard_stage(); result=-1; }
    return result;
}
#endif
#if !defined(PROX_NATIVE_RESIDENT_QUERY) || defined(PROX_NATIVE_DEDICATED_OTA)
static void close_report(void)
{
    if(report_fd>=0 && sceIoClose(report_fd)<0) report_failed=1;
    report_fd=-1;
}
int prox_native_handoff_close(void)
{ close_report();return report_failed?-1:0; }
#endif
void prox_native_worker(int argc,char **argv)
{
    ProxNativeQueryMessage m; unsigned int i; int valid=1;
    if(argc!=3 || !argv || !argv[0] || !argv[2] || strlen(argv[2])!=8) valid=0;
#ifdef PROX_NATIVE_RESIDENT_QUERY
    if(valid && strcmp(argv[0],PROX_RESIDENT_WORKER_ARG)) valid=0;
#else
    if(valid && strcmp(argv[0],"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP") &&
       strcmp(argv[0],"ef0:/PSP/GAME/PROXUPDATE/EBOOT.PBP")) valid=0;
#endif
    for(i=0;valid && i<8;++i) {
        char c=argv[2][i]; unsigned int n;
        if(c>='0' && c<='9') n=(unsigned int)(c-'0');
        else if(c>='a' && c<='f') n=(unsigned int)(c-'a'+10);
        else { valid=0; break; }
        lease_id=(lease_id<<4)|n;
    }
    if(!valid || !lease_id) { for(;;) sceKernelDelayThread(1000000); }
#ifndef PROX_NATIVE_RESIDENT_QUERY
    strcpy(directory,argv[0]); *strrchr(directory,'/')=0;
    snprintf(ca_path,sizeof(ca_path),"%s/cacert.pem",directory);
#endif
    /* The compiled immutable bundle survives card removal. Never replace TLS
     * validation with a cached catalog verdict or an unauthenticated query. */
    int trust_error=prox_native_trust();
#ifndef PROX_NATIVE_DEDICATED_OTA
    if(trust_error) { for(;;) sceKernelDelayThread(1000000); }
#endif
    packet(&m);
    if(proxNativeQueryService(PROX_NQ_REGISTER,&m,sizeof(m))<0) { for(;;) sceKernelDelayThread(1000000); }
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(trust_error)prox_native_resident_fault();
#endif
    for(;;) {
        packet(&m);
        if(proxNativeQueryService(PROX_NQ_POLL,&m,sizeof(m))<0) prox_native_resident_fault();
#ifdef PROX_NATIVE_DEDICATED_OTA
        if(m.command==PROX_NQ_BOOT_CLEANUP) {
            generation=m.generation;
            cleanup_verified_boot();
            packet(&m);
            if(proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))<0)prox_native_resident_fault();
            continue;
        }
#endif
        if(m.command==PROX_NQ_CHECK) {
            int result;
#ifndef PROX_NATIVE_RESIDENT_QUERY
            discard_stage(); have_offer=0;
#endif
            memset(&retained,0,sizeof(retained)); generation=m.generation;
            result=query(&retained);
#ifdef PROX_NATIVE_DEDICATED_SETTINGS
            have_dedicated_offer=result==1;
#endif
#ifndef PROX_NATIVE_RESIDENT_QUERY
            have_offer=result==1; offer_generation=generation;
#endif
            packet(&m); m.result=result; m.failure=failure; m.pages=retained.pages; m.candidates=retained.candidates;
            m.package_size=retained.manifest.package_size;
            memcpy(m.version,retained.manifest.version,sizeof(m.version));
            memcpy(m.summary,retained.manifest.summary,sizeof(m.summary));
#ifdef PROX_NATIVE_DEDICATED_OTA
            if(result==1)
                m.downloaded=write_storage_offer(m.summary,m.package_size);
#endif
            if(proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))<0) prox_native_resident_fault();
#ifdef PROX_NATIVE_DEDICATED_SETTINGS
        } else if(m.command==PROX_NQ_DOWNLOAD) {
            int result=-1;
            if(have_dedicated_offer && m.generation==generation && !prox_native_is_cancelled())
                result=dedicated_transfer();
            have_dedicated_offer=0;
            packet(&m);
            if(result || proxNativeQueryService(PROX_NQ_STAGED,&m,sizeof(m))<0)
                dedicated_finish();
        } else if(m.command==PROX_NQ_INSTALL) {
            if(m.generation==generation && dedicated_handoff() && !failure)
                failure=PROX_NQ_CAUSE_CLIENT;
            dedicated_finish();
        } else if(m.cancel && have_dedicated_stage) {
            dedicated_finish();
#endif
#ifndef PROX_NATIVE_RESIDENT_QUERY
        } else if(m.command==PROX_NQ_DOWNLOAD) {
            int rc=m.generation==offer_generation?download():-1;
            packet(&m); m.result=-1; m.failure=failure;
            if(!rc && proxNativeQueryService(PROX_NQ_STAGED,&m,sizeof(m))==0) {
                prox_log("Verified private stage ready; native writer cannot complete or launch a Sony updater");
            } else {
                discard_stage(); close_report(); have_offer=0;
                if(proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))<0) prox_native_resident_fault();
            }
        } else if(m.command==PROX_NQ_INSTALL) {
            int rc=-1;
            failure=PROX_NQ_CAUSE_CLIENT;
            if(have_offer && have_stage && m.generation==offer_generation && !prox_native_is_cancelled()) {
              if(!prox_power_ready()) failure=PROX_NQ_CAUSE_POWER;
              else
#ifdef PROX_NATIVE_INSTALL_PREVIEW
              {
                prox_log("NO-FLASH preview: keep original progress visible; module load and CFW restart only");
                close_report();
                if(!report_failed && !prox_native_is_cancelled()) rc=prox_install_preview_launch(directory);
              }
#else
#ifdef PROX_NATIVE_BACKGROUND
              if(have_native && !prox_stage_check(job.root,&retained.manifest)) {
                rc=prox_native_background_launch(directory,&native_job);
                close_report();
                if(!rc)rc=prox_native_background_wait();
              }
#else
              if(!prox_stage_check(job.root,&retained.manifest)) {
                prox_log("Verified native handoff to target installer %s; legacy --prox-ota-v1; settings/plugins preserved",retained.manifest.version);
                close_report();
                if(!report_failed && !prox_native_is_cancelled()) rc=prox_launch_job(&job);
              }
#endif
#endif
            }
            /* Successful LoadExec does not return. Preserve stage on a returned
             * launch for diagnosis; never report installation/reboot success. */
            (void)rc; close_report(); have_offer=0; have_stage=0;
#ifdef PROX_NATIVE_BACKGROUND
            have_native=0; /* Retain an attempted install's stage for recovery. */
#endif
            packet(&m); m.result=-1; m.failure=failure;
            if(proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))<0) prox_native_resident_fault();
        } else if(m.cancel && have_stage) {
            discard_stage(); have_offer=0; close_report();
            packet(&m); m.result=-1;
            if(proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))<0) prox_native_resident_fault();
#endif
        }
        sceKernelDelayThread(50000);
    }
}
