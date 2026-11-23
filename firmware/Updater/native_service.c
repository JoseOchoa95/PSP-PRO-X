/* Resident user-mode update worker. No renderer or HOME callback.
 * Own DNS/socket resources only; borrowed XMB networking is never terminated.
 * GPL-3.0-or-later. */
#ifdef PROX_NATIVE_SERVICE_TEST
#include "native_service_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#endif
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "native_service.h"
#include "prox_native_query.h"
#include "prox_product.h"
#include "bridge.h"
#include "transport.h"
#include "network_session.h"
#include "catalog.h"
#include "native_download.h"
#include "stage.h"
#ifdef PROX_NATIVE_BACKGROUND
#include "native_background.h"
static ProxNativeInstallJob native_job;
static int have_native;
#endif
#ifdef PROX_NATIVE_INSTALL_PREVIEW
#include "prox_install_preview.h"
#include "native_install_preview.h"
#endif
const char prox_native_service_marker[]="PROX-NATIVE-QUERY-V3";
static unsigned int lease_id,generation;
static int report_fd=-1,report_failed,report_required;
static char directory[80],ca_path[100];
static ProxCatalogOffer retained;
static ProxJob job;
static unsigned int offer_generation,running[3],model;
static int have_offer,have_stage;
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
static int open_report(const char *kind)
{
    char path[128]; unsigned int i; SceIoStat stat;
    report_failed=0;
    for(i=1;i<=999;++i) {
        snprintf(path,sizeof(path),"%s/native-%s-%03u.txt",directory,kind,i);
        if(sceIoGetstat(path,&stat)>=0) continue;
        report_fd=sceIoOpen(path,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_EXCL,0600);
        return report_fd<0?-1:0;
    }
    return -1;
}
static int query(ProxCatalogOffer *offer)
{
    int i,result=-1,borrowed=0; time_t now;
    failure=PROX_NQ_CAUSE_NONE;
    /* Checking is read-only and uses resident code/trust. An optional trace
     * cannot make absence of download storage a query prerequisite. */
    report_required=0;
    (void)open_report("report");
    prox_log("PRO-X v" PROX_PRODUCT_VERSION " native REAL query build " PROX_BUILD_ID);
    prox_log("%s; original XMB UI; checking does not authorize package download",prox_native_service_marker);
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
    result=prox_catalog_query(ca_path,running,model,offer);
    if(result==1) prox_log("Authenticated compatible target %s retained for this generation; waiting for native consent",offer->manifest.version);
done:
    if(borrowed && prox_network_end()) result=-1;
    if(prox_native_is_cancelled()) result=-1;
    prox_log("REAL native query completed result=%d pages=%u candidates=%u cause=%u",result,offer->pages,offer->candidates,failure);
    if(report_fd>=0) sceIoClose(report_fd);
    report_fd=-1; return result;
}
static int progress(unsigned int received,unsigned int total,void *context)
{
    ProxNativeQueryMessage m;
    (void)context;
    if(total!=retained.manifest.package_size || prox_native_is_cancelled()) return -1;
    packet(&m); m.downloaded=received;
    return proxNativeQueryService(PROX_NQ_PROGRESS,&m,sizeof(m));
}
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
static void close_report(void)
{
    if(report_fd>=0 && sceIoClose(report_fd)<0) report_failed=1;
    report_fd=-1;
}
int prox_native_handoff_close(void)
{ close_report();return report_failed?-1:0; }
void prox_native_worker(int argc,char **argv)
{
    ProxNativeQueryMessage m; unsigned int i; int valid=1;
    if(argc!=3 || !argv || !argv[0] || !argv[2] || strlen(argv[2])!=8) valid=0;
    if(valid && strcmp(argv[0],"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP") &&
       strcmp(argv[0],"ef0:/PSP/GAME/PROXUPDATE/EBOOT.PBP")) valid=0;
    for(i=0;valid && i<8;++i) {
        char c=argv[2][i]; unsigned int n;
        if(c>='0' && c<='9') n=(unsigned int)(c-'0');
        else if(c>='a' && c<='f') n=(unsigned int)(c-'a'+10);
        else { valid=0; break; }
        lease_id=(lease_id<<4)|n;
    }
    if(!valid || !lease_id) { for(;;) sceKernelDelayThread(1000000); }
    strcpy(directory,argv[0]); *strrchr(directory,'/')=0;
    snprintf(ca_path,sizeof(ca_path),"%s/cacert.pem",directory);
    /* The compiled immutable bundle survives card removal. Never replace TLS
     * validation with a cached catalog verdict or an unauthenticated query. */
    if(prox_native_trust()) { for(;;) sceKernelDelayThread(1000000); }
    packet(&m);
    if(proxNativeQueryService(PROX_NQ_REGISTER,&m,sizeof(m))<0) { for(;;) sceKernelDelayThread(1000000); }
    for(;;) {
        packet(&m);
        if(proxNativeQueryService(PROX_NQ_POLL,&m,sizeof(m))<0) prox_native_resident_fault();
        if(m.command==PROX_NQ_CHECK) {
            int result;
            discard_stage(); have_offer=0;
            memset(&retained,0,sizeof(retained)); generation=m.generation;
            result=query(&retained);
            have_offer=result==1; offer_generation=generation;
            packet(&m); m.result=result; m.failure=failure; m.pages=retained.pages; m.candidates=retained.candidates;
            m.package_size=retained.manifest.package_size;
            memcpy(m.version,retained.manifest.version,sizeof(m.version));
            memcpy(m.summary,retained.manifest.summary,sizeof(m.summary));
            if(proxNativeQueryService(PROX_NQ_RESULT,&m,sizeof(m))<0) prox_native_resident_fault();
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
        }
        sceKernelDelayThread(50000);
    }
}
