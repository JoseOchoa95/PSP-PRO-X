/* Actual engine orchestration, mocked device services; ordinary temporary files only. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <sys/stat.h>
#include "prox_native_install.h"
#include "prox_completion_preview.h"
#include "prox_product.h"
#include "native_flash.h"
#include "native_payload.h"
#include "ota_installer.h"
#ifdef PROX_NATIVE_DEDICATED_OTA
#include "dedicated_writer.h"
#endif
#undef main
int prox_native_engine_main(int,char **);
int prox_native_engine_write_gate(void);
extern unsigned int sce_newlib_attribute;
static int scenario,claimed,writing,prepared,swapped,stored,finalized,marked,committed,finished,starts,retries,rollbacks,failed,closed,polls,readonly_calls,access_calls;
static jmp_buf restarted;
static const char *root="/tmp/prox-native-engine-fixture";
const char *const prox_native_payload_names[11]={"SystemControl","VshCtrl","PROGalaxyController","stargate","pspMarch33_Driver","PRO_Inferno_Driver","pspUsbDev_Driver","PROPopcornManager","VshCtrlSatelite","Recovery","fast_recovery"};
int proxNativeInstallService(int action,ProxNativeInstallMessage *m,unsigned int size)
{
    unsigned int i;assert(size==sizeof(*m) && m->protocol==PROX_NI_PROTOCOL && m->lease==77 && m->generation==2);
    if(action==PROX_NI_CLAIM) {
        assert(!claimed && m->module==17);if(scenario==1)return -1;claimed=1;
        strcpy(m->job.root,root);strcpy(m->job.source_version,PROX_PRODUCT_VERSION);strcpy(m->job.target_version,"0.3.5");
        strcpy(m->job.build_id,"pro-x-123456789abc");m->job.model=1;
        for(i=0;i<11;++i) { m->job.parts[i].size=1;memset(m->job.parts[i].sha256,'a',64); }
#ifdef PROX_NATIVE_DEDICATED_OTA
        m->worker.size=633678;m->package.size=980000;
#endif
    } else if(action==PROX_NI_WRITE) { assert(claimed && !writing);writing=1; }
    else if(action==PROX_NI_ACCESS_CAPS){assert(claimed && !writing);m->result=scenario==22?2:PROX_NI_ACCESS_REVISION;}
    else if(action==PROX_NI_ACCESS_BEGIN){assert(writing && !finalized);++access_calls;m->result=(scenario==6 || (scenario==23 && access_calls==2))?-1:0;m->access.active=!m->result;}
    else if(action==PROX_NI_ACCESS_END){assert(writing);++readonly_calls;m->result=(scenario==12 || (scenario==24 && finalized) || (scenario==25 && swapped))?-1:0;m->access.active=!!m->result;m->access.restored=!m->result;}
    else if(action==PROX_NI_COMMIT) {
#ifdef PROX_NATIVE_DEDICATED_OTA
        assert(writing && swapped && !stored && !finalized && marked && readonly_calls==2 && access_calls==2);
#else
        assert(writing && stored && finalized && marked && readonly_calls);
#endif
        if(scenario==14)return -1;
        committed=1;
    }
    else if(action==PROX_NI_FAIL) { assert(!starts);failed=1; }
    else { assert(action==PROX_NI_STATUS);m->cancel=0;m->phase=PROX_NI_CLAIMED; }
    return 0;
}
unsigned int sceKernelDevkitVersion(void) { return 0x06060110; }
int proxUpdateGetState(int k)
{ int value[]={1,PROX_PRODUCT_MAJOR,PROX_PRODUCT_MINOR,PROX_PRODUCT_PATCH,0x06060110,1};return scenario==19 && k==1?99:value[k]; }
int kuKernelGetModel(void) { return 1; }
int kuKernelGetUserLevel(void) { return scenario==20?0:scenario==21?8:4; }
int prox_ota_native_begin(const ProxNativeInstallJob *j,unsigned int fw,unsigned int model)
{ assert(claimed && !writing && !strcmp(j->root,root) && fw==0x06060110 && model==1);return scenario==2?-1:1; }
int prox_stopping(void) { return 0; }
int prox_file_check(const char *p,unsigned int n,const char *hash,int executable)
{ assert(strstr(p,"/native/") && n==1 && hash[0]=='a' && !executable);return scenario==3?-1:0; }
int prox_native_image(const unsigned char *data,size_t n,char name[28],unsigned int *extent)
{ assert(n==1 && data[0]<11);strcpy(name,prox_native_payload_names[data[0]]);*extent=1;return 0; }
int kuKernelLoadModule(const char *p,int flags,void *options)
{ assert(strstr(p,"/native/10.prx") && !flags && !options && !writing);return scenario==4?-1:19; }
int sceKernelUnloadModule(int module) { assert(module==19 && !committed);return 0; }
int prox_ota_before_write(void) { return scenario==5?-1:prox_native_engine_write_gate(); }
int sceIoUnassign(const char *p) { (void)p;assert(!"Actual writer must not unassign live flash0");return -1; }
int sceIoAssign(const char *p,const char *block,const char *fat,int mode,void *arg,int n)
{
    (void)p;(void)block;(void)fat;(void)mode;(void)arg;(void)n;assert(!"Actual writer must not reassign live flash0");return -1;
}
int sceIoSync(const char *p,int mode) { assert(writing && !strcmp(p,"flash0:") && !mode);return scenario==9?-1:0; }
int prox_native_flash_prepare(const ProxNativeInstallJob *j,ProxNativeFlash *t)
{ (void)j;(void)t;assert(writing && !swapped);prepared=1;return scenario==7?-1:0; }
int prox_native_flash_commit(const ProxNativeInstallJob *j,ProxNativeFlash *t)
{ (void)j;(void)t;assert(prepared && !stored && readonly_calls==1 && access_calls==2);swapped=1;return scenario==8?-1:0; }
int prox_native_flash_rollback(const ProxNativeInstallJob *j,ProxNativeFlash *t)
{ (void)j;(void)t;assert(writing && !finalized);++rollbacks;return 0; }
int prox_native_flash_finalize(const ProxNativeInstallJob *j,ProxNativeFlash *t)
{ (void)j;(void)t;assert(stored && !marked);finalized=1;return scenario==11?-1:0; }
int prox_ota_storage_commit(void) { assert(swapped && !finalized && readonly_calls==2 && access_calls==3);if(scenario==10)return -1;stored=1;return 0; }
int prox_ota_mark_committed(void) { assert(stored && finalized && readonly_calls);if(scenario==13)return -1;marked=1;return 0; }
#ifdef PROX_NATIVE_DEDICATED_OTA
int prox_ota_dedicated_trace(const ProxDedicatedJob *d)
{ assert(claimed && !writing && !strcmp(d->install.root,root));return scenario==2?-1:0; }
int prox_ota_dedicated_lock(void) { return prox_ota_before_write(); }
int prox_dedicated_writer_check(const ProxDedicatedJob *d,unsigned int lease,unsigned int generation)
{ assert(lease==77 && generation==2 && d->worker.size==633678 && d->package.size==980000 && !writing);return scenario==3?-1:0; }
int prox_dedicated_record_available(const ProxDedicatedJob *d)
{ assert(!writing && !strcmp(d->install.root,root));return scenario==26?-1:0; }
int prox_dedicated_writer_prepare(const ProxDedicatedJob *d,unsigned int lease,unsigned int generation,ProxNativeFlash *t)
{
    assert(lease==77 && generation==2);
    int result=prox_native_flash_prepare(&d->install,t);
    if(!result)t->prepared=1;
    return result;
}
int prox_dedicated_record_commit(const ProxDedicatedJob *d,const ProxNativeFlash *transaction)
{
    assert(transaction->prepared && !strcmp(d->install.root,root) && swapped && !stored && !finalized && readonly_calls==2 && access_calls==2);
    if(scenario==10)return -1;
    marked=1;return 0;
}
#endif
void prox_log(const char *fmt,...) { assert(fmt); }
void prox_ota_close(void) { ++closed; }
int proxNativeCompletionControl(int action)
{
    assert(committed && readonly_calls && marked);
    if(action==PROX_COMPLETION_FINISH) { if(scenario==15)return -1;finished=1;return 0; }
    if(action==PROX_COMPLETION_RETRY) { assert(starts==1 && scenario==18);++retries;return 0; }
    assert(action==PROX_COMPLETION_POLL && finished);++polls;
    if(scenario==16)return -1;
    if(scenario==17)return 0;
    /* Long completion wait is allowed, without timed restart. */
    if(polls<1600 || (scenario==18 && retries && polls<1700))return 1;
    return 2;
}
int scePowerTick(int n) { assert(!n && committed);return 0; }
int sceKernelDelayThread(unsigned int us) { assert(us==20000 || (us==3000000 && scenario==18 && starts==1));return 0; }
int sceKernelStartModule(int module,unsigned int size,void *args,int *status,void *options)
{
    assert(module==19 && size==sizeof(int) && *(int *)args==1 && !options && committed && finished && polls>=1600 && closed);
    ++starts;*status=0;if(scenario==18 && starts==1)return -1;
    longjmp(restarted,1);
}
int main(void)
{
    unsigned int i;char path[256];char *args[]={path,PROX_NI_ARGUMENT,"0000004d","00000002","00000011"};
    assert(sce_newlib_attribute==0xc0004000u);
    mkdir(root,0700);snprintf(path,sizeof(path),"%s/native",root);mkdir(path,0700);
    for(i=0;i<11;++i) { FILE *f;snprintf(path,sizeof(path),"%s/native/%02u.prx",root,i);f=fopen(path,"wb");assert(f);assert(fputc(i,f)!=EOF);assert(!fclose(f)); }
    snprintf(path,sizeof(path),"%s/native/engine.prx",root);
    for(scenario=0;scenario<27;++scenario) {
#ifdef PROX_NATIVE_DEDICATED_OTA
        if(scenario==11 || scenario==13 || scenario==23 || scenario==24)continue;
#else
        if(scenario==26)continue;
#endif
        claimed=writing=prepared=swapped=stored=finalized=marked=committed=finished=starts=retries=rollbacks=failed=closed=polls=readonly_calls=access_calls=0;
        if(!setjmp(restarted)) { assert(prox_native_engine_main(5,args)==-1);assert(scenario!=0 && scenario!=18); }
        if(scenario==0 || scenario==18)assert(starts==(scenario==18?2:1) && !failed && committed && polls>=1600);
        else { assert(!starts);assert(failed==(scenario!=1)); }
        if((scenario>=7 && scenario<=10) || scenario==12 || scenario==25
#ifdef PROX_NATIVE_DEDICATED_OTA
           || scenario==14
#endif
          )assert(rollbacks==1);
        else assert(!rollbacks);
        if((scenario>0 && scenario<7) || (scenario>=19 && scenario<=22))assert(!finished && !prepared && !stored && !committed);
        if(scenario==23)assert(prepared && !swapped && !stored && !committed && readonly_calls==1);
        if(scenario==20 || scenario==21)assert(!writing);
#ifdef PROX_NATIVE_DEDICATED_OTA
        if(scenario==26)assert(!writing && !access_calls);
        if(scenario==10 || scenario==14)assert(access_calls==3 && readonly_calls==3 && !committed);
        assert(!finalized && !stored);
#endif
    }
    puts("PASS: VSH thread/level and resident revision guards; BEGIN/END access lease, no live unassign/assign, checked restore before mark/commit/completion; actual engine failure/rollback/fresh-Cross gates; no device writes");
#ifdef PROX_NATIVE_DEDICATED_OTA
    puts("PASS: v2 writer retains flash backups and card stage; checked flash restoration precedes the dedicated commit record and fresh-Cross completion");
#endif
    return 0;
}
