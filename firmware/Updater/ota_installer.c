/* Installer-side OTA validation and storage replacement. GPL-3.0-or-later.
 * The launcher forwards the verified manifest digest as a copied argument.
 * Every executable is verified again before any flash operation. */
#ifdef PROX_OTA_NATIVE
#include "ota_installer_shim.h"
#else
#include <pspkernel.h>
#include <psppower.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspctrl.h>
#endif
#include "ota_installer.h"
#include "update.h"
#include "stage.h"
#include "policy.h"
#include "prox_product.h"
#include "prox_ota_args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static char root[192],game[32];
static ProxManifest manifest;
static FILE *trace;
static int locked,trace_failed;
int prox_stopping(void)
{
    SceCtrlData pad; int rc;
    if (locked) return 0; /* No cancellation during firmware/storage commit. */
    memset(&pad,0,sizeof(pad)); rc=sceCtrlPeekBufferPositive(&pad,1);
    return rc < 0 || (rc > 0 && (pad.Buttons & PSP_CTRL_RTRIGGER));
}
void prox_log(const char *format,...)
{
    char b[384]; va_list args;
    va_start(args,format); vsnprintf(b,sizeof(b),format,args); va_end(args);
    pspDebugScreenPrintf("%s\n",b);
    if (trace && (fprintf(trace,"%s\n",b) < 0 || fflush(trace))) trace_failed=1;
}
static int trace_open(void)
{
    char name[96]; FILE *f; long size;
    snprintf(name,sizeof(name),"%s/PROXUPDATE/install.txt",game);
    f=fopen(name,"rb");
    if (f) { if (fseek(f,0,SEEK_END) || (size=ftell(f)) < 0 || size > 60000) { fclose(f); return -1; } fclose(f); }
    trace=fopen(name,"ab"); return trace ? 0 : -1;
}
static int pending(int committed)
{
    char name[96]; FILE *f; int rc;
    snprintf(name,sizeof(name),"%s/PROXUPDATE/installed.pending",game);
    if (!committed) { f=fopen(name,"rb"); if (f) { fclose(f); return -1; } }
    f=fopen(name,"wb"); if (!f) return -1;
    rc=fprintf(f,"PROX-OTA-1\n%s\n%s\n%s\n",manifest.version,manifest.build_id,committed ? "committed" : "prepared") < 0 ? -1 : 0;
    if (fclose(f)) rc=-1;
    if (rc) remove(name); /* This invocation created it after absence check. */
    return rc;
}
int prox_ota_begin(int argc,char **argv,unsigned int firmware,unsigned int model)
{
    size_t n; char name[256],*data=NULL; PjToken *tokens=NULL; Pj json;
    ProxRelease release; unsigned int running[3],client[3]; FILE *f=NULL; int i,rc=-1;
    i=prox_ota_args(argc,argv,"/PROUPDATE/installer.prx",root);
    if (i <= 0) return i;
    memset(&release,0,sizeof(release));
    if (firmware != 0x06060110) return -1;
    memcpy(game,root,4); strcpy(game+4,"/PSP/GAME");
    if (trace_open() || prox_version(argv[3],running)) goto done;
    memcpy(client,running,sizeof(client));
    strcpy(release.version,PROX_PRODUCT_VERSION);
    snprintf(release.tag,sizeof(release.tag),"v%s",PROX_PRODUCT_VERSION);
    snprintf(name,sizeof(name),"%s/pro-x-update.json",root);
    f=fopen(name,"rb"); if (!f) goto done;
    data=malloc(32769); tokens=calloc(2048,sizeof(*tokens));
    if (!data || !tokens) goto done;
    n=fread(data,1,32769,f);
    if (ferror(f) || n > 32768) goto done;
    if (fclose(f)) { f=NULL; goto done; } f=NULL;
    if (prox_file_check(name,(unsigned int)n,argv[2],0) || pj_parse(&json,data,n,tokens,2048) ||
        pj_uint(&json,pj_field(&json,pj_field(&json,0,"package_asset"),"size"),&release.package.size) ||
        prox_manifest(&json,&release,running,client,model,&manifest) ||
        strncmp(strrchr(root,'/')+1+7,manifest.build_id,18) || prox_stage_check(root,&manifest) || !prox_power_ready()) goto done;
    prox_log("Verified OTA installer for %s / %s; preserving configuration and plugins",manifest.version,manifest.build_id);
    snprintf(name,sizeof(name),"%s/PROUPDATE",root);
    if (sceIoChdir(name) < 0) goto done;
    rc=1;
done:
    if (f) fclose(f);
    free(tokens); free(data);
    if (rc < 0) prox_log("OTA validation failed before flash setup; nothing installed");
    return rc;
}
int prox_ota_before_write(void)
{
    unsigned long long required=2u*1024u*1024u; int i;
    for (i=0;i<PROX_UPDATE_FILES;++i) required+=manifest.files[i].size;
    if (trace_failed || !prox_power_ready() || prox_storage_space(game,required) || prox_stage_check(root,&manifest)) return -1;
    /* The updater has exited. Adjacent .prox-new files do not replace originals. */
    if (prox_storage_prepare(root,game,&manifest)) return -1;
    if (pending(0)) { prox_storage_discard(game,&manifest); return -1; }
    if (prox_stopping() || !prox_power_ready() || scePowerLock(0) < 0) {
        char name[96];
        snprintf(name,sizeof(name),"%s/PROXUPDATE/installed.pending",game); remove(name);
        prox_storage_discard(game,&manifest); return -1;
    }
    locked=1;
    prox_log("OTA preflight PASS; firmware write starts, cancellation is disabled");
    if (trace_failed) {
        char name[96];
        snprintf(name,sizeof(name),"%s/PROXUPDATE/installed.pending",game); remove(name);
        prox_storage_discard(game,&manifest); prox_ota_close(); return -1;
    }
    return 0;
}
int prox_ota_complete(void)
{
    if (prox_storage_commit(game,&manifest)) {
        prox_log("Storage replacement failed after firmware write. Keep stage/backups and use the matched recovery package.");
        return -1;
    }
    prox_log("Matched applications replaced; pending version observation after CFW restart");
    return pending(1);
}
void prox_ota_result(int result) { prox_log("Installer firmware-write result=0x%08x",result); }
int prox_ota_reboot_path(char path[256])
{
    int n;
    if (!root[0]) return 0;
    n=snprintf(path,256,"%s/PROUPDATE/Rebootex.prx",root);
    return n < 0 || n >= 256 ? -1 : 1;
}
int prox_ota_cleanup_modules(void)
{
    char path[256];
    if (!root[0]) return 0;
    snprintf(path,sizeof(path),"%s/PROUPDATE/installer.prx",root); sceIoRemove(path);
    snprintf(path,sizeof(path),"%s/PROUPDATE/Rebootex.prx",root); sceIoRemove(path);
    return 1;
}
void prox_ota_close(void)
{
    if (locked) { scePowerUnlock(0); locked=0; }
    if (trace) { fclose(trace); trace=NULL; }
}
