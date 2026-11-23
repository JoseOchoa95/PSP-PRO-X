/* No-flash VSH background lifecycle diagnostic. GPL-3.0-or-later.
 * It owns no framebuffer or replacement UI and contains no install function. */
#ifdef PROX_INSTALL_PREVIEW_TEST
#include "install_preview_shim.h"
#else
#include <pspkernel.h>
#include <psppower.h>
#include "kubridge.h"
#endif
#include <stdio.h>
#include <string.h>
#include "prox_product.h"
#include "prox_install_preview.h"
#include "prox_completion_preview.h"
PSP_MODULE_INFO(PROX_INSTALL_PREVIEW_MODULE,0x0800,1,0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_STACK_SIZE_KB(32);
PSP_HEAP_SIZE_KB(64);
const char prox_preview_capability[]="PROX-NATIVE-INSTALL-PREVIEW-NO-FLASH-V1";
const char prox_completion_capability[]="PROX-NATIVE-COMPLETION-PREVIEW-V1";
const char prox_completion_button_capability[]="PROX-NATIVE-COMPLETION-X-ONLY-V1";
static FILE *report;
static int line(const char *text,int value)
{
    return !report || fprintf(report,"%s %d\n",text,value)<0 || fflush(report)?-1:0;
}
int main(int argc,char **argv)
{
    char path[160]; unsigned int i; int lock=0,module,rc,status=-1;
    if(argc!=3 || !argv || !argv[0] || !argv[1] ||
       strcmp(argv[1],PROX_INSTALL_PREVIEW_ARGUMENT) || !prox_install_preview_path(argv[2]) ||
       sceKernelDevkitVersion()!=0x06060110u) return -1;
    snprintf(path,sizeof(path),"%s/native-install-preview.prx",argv[2]);
    if(strcmp(argv[0],path)) return -1;
    for(i=1;i<=999;++i) {
        FILE *old;
        snprintf(path,sizeof(path),"%s/install-preview-%03u.txt",argv[2],i);
        old=fopen(path,"rb"); if(old) { fclose(old); continue; }
        report=fopen(path,"wb"); break;
    }
    if(!report) return -1;
    if(line("PRO-X " PROX_PRODUCT_VERSION " ORIGINAL XMB BACKGROUND PREVIEW; NO FLASH",1)) goto done;
    lock=scePowerLock(0)>=0;
    if(!lock || line("START; original progress must stay visible; no second confirmation",0)) goto done;
    for(i=1;i<=8;++i) {
        scePowerTick(0);
        if(line("NO-FLASH background step",(int)i)) goto done;
        sceKernelDelayThread(1000000);
    }
    if(proxNativeCompletionControl(PROX_COMPLETION_FINISH)<0 || line("NO-FLASH completion ready; awaiting original native completion page",0)) goto done;
    for(i=0;i<250;++i) {
        rc=proxNativeCompletionControl(PROX_COMPLETION_POLL);
        if(rc<0) goto done;
        if(rc>=1) break;
        sceKernelDelayThread(20000);
    }
    if(i==250 || line("Original completion text requested; waiting for Cross only; native Back hidden; no automatic restart timer",0)) goto done;
    while(rc!=2) {
        scePowerTick(0);
        sceKernelDelayThread(20000);
        rc=proxNativeCompletionControl(PROX_COMPLETION_POLL);
        if(rc<0) goto done;
    }
    if(line("Cross requested CFW restart; no installed-version change",0)) goto done;
    snprintf(path,sizeof(path),"%s/native-preview-reboot.prx",argv[2]);
    module=kuKernelLoadModule(path,0,NULL);
    if(line("matching reboot module load",module) || module<0) goto done;
    { int mode=1; rc=sceKernelStartModule(module,sizeof(mode),&mode,&status,NULL); }
    if(rc<0 || status<0) { line("reboot module failed",rc<0?rc:status); goto done; }
    line("restart module accepted; observation of XMB restart is a PSP test",0);
    /* The existing matching PRO rebooter invokes sctrlKernelExitVSH. Do not
     * issue a second ExitGame/LoadExec or fabricate a successful install. */
    fclose(report); report=NULL;
    sceKernelDelayThread(3000000);
done:
    proxNativeCompletionControl(PROX_COMPLETION_FAIL);
    if(report) { line("PREVIEW RETURNED; no firmware was written",-1); fclose(report); }
    if(lock) scePowerUnlock(0);
    return -1;
}
