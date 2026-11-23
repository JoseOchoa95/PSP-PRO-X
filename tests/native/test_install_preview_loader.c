/* Actual loader control flow; no filesystem resource copies or PSP code run. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int mode,checks,prepares,loads,starts,polls,delays;
static FILE *resource_open(const char *path,const char *access)
{
    assert(!strcmp(path,"ms0:/PSP/GAME/PROXUPDATE/native-completion.rco") && !strcmp(access,"rb"));
    return tmpfile();
}
#define fopen resource_open
#include "../../firmware/Updater/native_install_preview.c"
#undef fopen
int prox_file_check(const char *p,unsigned int n,const char *hash,int pbp)
{
    ++checks;assert(!pbp);
    if(strstr(p,"native-install-preview.prx")) assert(n==101 && !strcmp(hash,PROX_PREVIEW_MODULE_SHA256));
    else if(strstr(p,"native-preview-reboot.prx")) assert(n==102 && !strcmp(hash,PROX_PREVIEW_REBOOT_SHA256));
    else { assert(strstr(p,"native-completion.rco") && n==PROX_COMPLETION_RCO_SIZE && !strcmp(hash,PROX_COMPLETION_RCO_SHA256));if(mode==1)return -1; }
    return 0;
}
int prox_native_is_cancelled(void) { return mode==2; }
int sceUtilityGetSystemParamInt(int id,int *language) { assert(id==8);*language=3;return 0; }
int prox_completion_read_text(FILE *f,unsigned int language,unsigned short *out,unsigned int capacity)
{ assert(f && language==3 && capacity==120);out[0]='A';out[1]=0;return 2; }
int prox_native_completion_prepare(const unsigned short *text,unsigned int count)
{ ++prepares;assert(text[0]=='A' && !text[1] && count==2);return 0; }
void prox_log(const char *format,...) { (void)format;assert(0); } /* Worker report is closed. */
int kuKernelLoadModule(const char *p,int flags,void *opt)
{ ++loads;assert(strstr(p,"native-install-preview.prx") && !flags && !opt);return mode==3?-1:17; }
int sceKernelStartModule(int module,unsigned int size,void *args,int *status,void *opt)
{ ++starts;assert(module==17 && size>50 && args && !opt);*status=mode==4?-1:0;return 0; }
int prox_native_completion_pending(void)
{
    ++polls;
    if(mode==5)return 0; /* Startup watchdog, no completion. */
    if(polls>6002)return -1; /* Probe eventually fails after >120 sec of user wait. */
    return polls==1?0:polls==2?1:2;
}
int sceKernelDelayThread(unsigned int us) { assert(us==20000);++delays;return 0; }
int main(void)
{
    for(mode=0;mode<6;++mode) {
        checks=prepares=loads=starts=polls=delays=0;
        assert(prox_install_preview_launch("ms0:/PSP/GAME/PROXUPDATE")<0);
        if(mode==1 || mode==2) assert(checks==3 && !prepares && !loads && !polls);
        else if(mode==3) assert(prepares==1 && loads==1 && !starts && !polls);
        else if(mode==4) assert(starts==1 && !polls);
        else if(mode==5) assert(polls==1250 && delays==1249);
        else assert(polls==6003 && delays==6002 && starts==1);
    }
    puts("PASS: actual preview loader rejects missing resource/cancel/load/start errors, bounds startup and waits beyond two minutes while completion probe is healthy; no fixed restart timer or closed-report logging");
}
