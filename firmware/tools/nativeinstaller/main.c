/* Local matched writer. Target binaries are authenticated data until Cross
 * starts the target's preloaded rebooter. No framebuffer/console/LoadExec. */
#ifdef PROX_NATIVE_ENGINE_TEST
#include "native_engine_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psppower.h>
#include "kubridge.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prox_native_install.h"
#include "prox_completion_preview.h"
#include "prox_product.h"
#include "native_flash.h"
#include "native_payload.h"
#include "ota_installer.h"
#include "stage.h"
#include "bridge.h"
#include "transport.h"
#ifdef PROX_NATIVE_DEDICATED_OTA
#include "dedicated_writer.h"
#endif
PSP_MODULE_INFO(PROX_NI_MODULE,0x0800,1,0);
/* Keep the CRT-created writer in the VSH API class of its 0x0800 module.
 * An ordinary USER main shares low-level per-device descriptor limits with
 * the live shell; do not raise privileges by patching another thread. */
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VSH | THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_STACK_SIZE_KB(64);
PSP_HEAP_SIZE_KB(512);
const char prox_native_writer_identity[]="PROX-NATIVE-INSTALL-WRITER-V1";
static ProxNativeInstallMessage message;
static int rpc(int action)
{ return proxNativeInstallService(action,&message,sizeof(message)); }
int prox_native_engine_cancelled(void)
{ return rpc(PROX_NI_STATUS)<0 || message.cancel || message.phase==PROX_NI_FAILED; }
int prox_native_engine_write_gate(void)
{
    if(proxUpdateGetState(0)!=1 || proxUpdateGetState(1)!=PROX_PRODUCT_MAJOR ||
       proxUpdateGetState(2)!=PROX_PRODUCT_MINOR || proxUpdateGetState(3)!=PROX_PRODUCT_PATCH ||
       proxUpdateGetState(4)!=0x06060110 || proxUpdateGetState(5)!=(int)message.job.model)return -1;
    return rpc(PROX_NI_WRITE);
}
static int parse_hex_word(const char *text, unsigned int *value)
{
    if (!text)
        return -1;

    unsigned int result = 0;
    for (unsigned int i = 0; i < 8; ++i) {
        unsigned int digit = (unsigned char)text[i];
        if (digit >= '0' && digit <= '9')
            digit -= '0';
        else if (digit >= 'a' && digit <= 'f')
            digit = digit - 'a' + 10;
        else
            return -1;
        result = (result << 4) | digit;
    }
    if (text[8])
        return -1;
    *value = result;
    return 0;
}
#ifndef PROX_NATIVE_DEDICATED_OTA
static int verify_parts(const ProxNativeInstallJob *job)
{
    unsigned int i;char path[256],name[28];
    for(i=0;i<PROX_NI_PARTS;++i) {
        FILE *file;unsigned char *bytes;unsigned int extent;int rc;
        if(prox_stopping() || !job->parts[i].size || job->parts[i].size>1024u*1024u ||
           snprintf(path,sizeof(path),"%s/native/%02u.prx",job->root,i)>=(int)sizeof(path) ||
           prox_file_check(path,job->parts[i].size,job->parts[i].sha256,0))return -1;
        bytes=malloc(job->parts[i].size);if(!bytes)return -1;
        file=fopen(path,"rb");if(!file) { free(bytes);return -1; }
        rc=fread(bytes,1,job->parts[i].size,file)==job->parts[i].size?0:-1;
        if(fclose(file))rc=-1;
        if(!rc)rc=prox_native_image(bytes,job->parts[i].size,name,&extent);
        free(bytes);
        if(rc || extent!=job->parts[i].size || strcmp(name,prox_native_payload_names[i]))return -1;
    }
    return 0;
}
#endif
static int writable(int *mounted)
{
    int transport;memset(&message.access,0,sizeof(message.access));message.result=-1;
    transport=rpc(PROX_NI_ACCESS_BEGIN);
    if(!transport)*mounted=message.access.active;
    prox_log("Background access BEGIN: rpc=%d result=%d stage=%d active=%d owned=%d refs=%u",
             transport,message.result,message.access.stage,message.access.active,message.access.owned_fd,message.access.refs_before);
    return transport || message.result || !*mounted?-1:0;
}
static int readonly(int *mounted)
{
    int transport=rpc(PROX_NI_ACCESS_END);
    prox_log("Background access END: rpc=%d result=%d stage=%d active=%d restored=%d flush=%d retained=%d flags=0x%08x refs=%u",
             transport,message.result,message.access.stage,message.access.active,message.access.restored,message.access.flush_result,
             message.access.retained_fd,message.access.flags_after,message.access.refs_after);
    if(transport || message.result || message.access.active || !message.access.restored)return -1;
    *mounted=0;return 0;
}

/* Returns only if the completion screen cannot continue. */
static void restart_after_confirmation(int rebooter)
{
    if (proxNativeCompletionControl(PROX_COMPLETION_FINISH) < 0)
        return;

    int completion = 0;
    for (unsigned int attempt = 0; attempt < 250; ++attempt) {
        completion = proxNativeCompletionControl(PROX_COMPLETION_POLL);
        if (completion < 0)
            return;
        if (completion >= 1)
            break;
        sceKernelDelayThread(20000);
    }
    if (completion == 0)
        return;

    for (;;) {
        while (completion != 2) {
            scePowerTick(0);
            sceKernelDelayThread(20000);
            completion = proxNativeCompletionControl(PROX_COMPLETION_POLL);
            if (completion < 0)
                return;
        }

        int mode = 1;
        int status = -1;
        prox_log("Cross requests verified target CFW restart; no additional installation confirmation");
        prox_ota_close();
        sceKernelStartModule(rebooter, sizeof(mode), &mode, &status, NULL);
        /* Activation is verified after reboot. If this code is still running,
         * retain the completion screen and wait for a fresh Cross press. */
        sceKernelDelayThread(3000000);
        if (proxNativeCompletionControl(PROX_COMPLETION_RETRY) < 0)
            return;
        completion = 1;
    }
}

int main(int argc,char **argv)
{
    char path[256];
    unsigned int module_number;
    int reboot = -1, mounted = 0, finalizing = 0, committed = 0;
    ProxNativeFlash transaction={0};const ProxNativeInstallJob *job=&message.job;
    memset(&message,0,sizeof(message));message.protocol=PROX_NI_PROTOCOL;
#ifdef PROX_NATIVE_DEDICATED_OTA
    int download_claimed=0,rollback_failed=0;
#endif
    if (argc != 5 || !argv || !argv[0] || !argv[1] || strcmp(argv[1], PROX_NI_ARGUMENT) ||
        parse_hex_word(argv[2], &message.lease) || parse_hex_word(argv[3], &message.generation) ||
        parse_hex_word(argv[4], &module_number) || module_number == 0 || module_number > INT_MAX)
        return -1;
    message.module = (int)module_number;
    if(rpc(PROX_NI_CLAIM))return -1;
    int level = kuKernelGetUserLevel();
    prox_log("Background writer thread: VSH API expected level=4 actual=%d; no global privilege changes",level);
    if(level!=4)goto failure;
    if(!memchr(job->root,0,sizeof(job->root)) ||
       snprintf(path,sizeof(path),"%s/native/engine.prx",job->root)>=(int)sizeof(path) || strcmp(argv[0],path) ||
       sceKernelDevkitVersion()!=0x06060110u || proxUpdateGetState(0)!=1 ||
       proxUpdateGetState(5)!=(int)job->model || kuKernelGetModel()!=(int)job->model)goto failure;
#ifdef PROX_NATIVE_DEDICATED_OTA
    ProxDedicatedJob dedicated={.install=*job,.worker=message.worker,.package=message.package};
    if(prox_ota_dedicated_trace(&dedicated) || prox_dedicated_writer_check(&dedicated,message.lease,message.generation) ||
       prox_dedicated_record_available(&dedicated))goto failure;
#else
    if(prox_ota_native_begin(job,0x06060110u,job->model)!=1 || verify_parts(job))goto failure;
#endif
    if(rpc(PROX_NI_ACCESS_CAPS) || message.result!=PROX_NI_ACCESS_REVISION) {
        prox_log("Background resident access revision mismatch; install/start the matching PROUPDATE before this test");goto failure;
    }
    /* Kernel target rebooter is loaded but never started until completion/X.
     * All code/memory needed for restart is prepared before flash changes. */
    snprintf(path,sizeof(path),"%s/native/10.prx",job->root);
    reboot = kuKernelLoadModule(path, 0, NULL);
    if (reboot <= 0)
        goto failure;
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(prox_ota_dedicated_lock() || prox_dedicated_begin_write(&dedicated))goto failure;
    download_claimed=1;
#else
    if(prox_ota_before_write())goto failure;
#endif
    if(writable(&mounted))goto failure;
    prox_log("Background firmware transaction begins; fixed PRO module paths only; no configuration/plugin/Infinity changes");
    int rc;
#ifdef PROX_NATIVE_DEDICATED_OTA
    rc=prox_dedicated_writer_prepare(&dedicated,message.lease,message.generation,&transaction);
#else
    rc=prox_native_flash_prepare(job,&transaction);
#endif
    prox_log("TRANSACTION stage=firmware-prepare rc=%d prepared=0x%03x changed=0x%03x originals=0x%03x identical=0x%03x",rc,transaction.prepared,transaction.changed,transaction.originals,transaction.identical);
    if(rc)goto failure;
    /* Persist every verified original backup before destructive opens. The
     * public global sync drops per-volume errors; END uses the checked native
     * flush under the original mutex, then BEGIN reacquires the same lease. */
    prox_log("TRANSACTION stage=backup-flush begins; no original replaced");
    if(readonly(&mounted) || writable(&mounted))goto failure;
    rc=prox_native_flash_commit(job,&transaction);
    prox_log("TRANSACTION stage=firmware-replace rc=%d prepared=0x%03x changed=0x%03x originals=0x%03x",rc,transaction.prepared,transaction.changed,transaction.originals);
    if(rc)goto failure;
    rc=sceIoSync("flash0:",0);prox_log("TRANSACTION stage=firmware-sync rc=0x%08x",(unsigned int)rc);if(rc<0)goto failure;
    /* Confirm target persistence while original backups still exist. */
    prox_log("TRANSACTION stage=target-flush begins; verified original backups retained");
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(readonly(&mounted))goto failure;
    rc=prox_dedicated_record_commit(&dedicated,&transaction);
    prox_log("TRANSACTION stage=dedicated-card-record rc=%d",rc);
    if(rc)goto failure;
#else
    if(readonly(&mounted) || writable(&mounted))goto failure;
    rc=prox_ota_storage_commit();prox_log("TRANSACTION stage=application-commit rc=%d",rc);if(rc)goto failure;
    finalizing=1;
    rc=prox_native_flash_finalize(job,&transaction);prox_log("TRANSACTION stage=firmware-finalize rc=%d",rc);if(rc)goto failure;
    rc=readonly(&mounted);if(rc)goto failure;
    rc=prox_ota_mark_committed();prox_log("TRANSACTION stage=mark-committed rc=%d",rc);if(rc)goto failure;
#endif
    if(rpc(PROX_NI_COMMIT))goto failure;
    committed=1;
#ifdef PROX_NATIVE_DEDICATED_OTA
    prox_log("Dedicated target %s/%s: firmware verified, flash0 readonly, backups retained for restart verification",job->target_version,job->build_id);
#else
    prox_log("Background target %s/%s: firmware bytes verified, applications committed, flash0 readonly; restart observation pending",job->target_version,job->build_id);
#endif
    restart_after_confirmation(reboot);
failure:
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(!committed && !mounted && (transaction.prepared || transaction.worker_prepared) && writable(&mounted)) {
        rollback_failed=1;
        prox_log("Could not reopen flash to roll back; preserve original backups and the card stage");
    }
#endif
    if(mounted) {
        if(!finalizing && prox_native_flash_rollback(job,&transaction)) {
#ifdef PROX_NATIVE_DEDICATED_OTA
            rollback_failed=1;
#endif
            prox_log("Background firmware rollback incomplete; keep exact stage and recovery package");
        }
        if(readonly(&mounted)) {
#ifdef PROX_NATIVE_DEDICATED_OTA
            rollback_failed=1;
#endif
            prox_log("Background readonly restoration incomplete; retain exact stage and recovery package; no success/reboot");
        }
    }
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(download_claimed && !committed && !rollback_failed && !prox_dedicated_record_available(&dedicated))
        (void)prox_dedicated_mark_download(dedicated.install.root);
#endif
    prox_log("Background installation/restart returned failure; committed=%d. No automatic reboot; retain stage/pending/backups",committed);
    prox_ota_close();
    if (!committed && reboot > 0)
        sceKernelUnloadModule(reboot);
    (void)rpc(PROX_NI_FAIL);
    return -1;
}
