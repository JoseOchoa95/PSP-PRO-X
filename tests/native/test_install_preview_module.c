/* Execute the actual no-flash module with host adapters, not a mirrored model. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FILE *opened;
static int mode,steps,loads,starts,unlocks,notice_calls,polls,fail_calls;
static char output[8192];
static FILE *probe_open(const char *path,const char *access)
{
    assert(strstr(path,"ms0:/PSP/GAME/PROXUPDATE/install-preview-")==path);
    if(!strcmp(access,"rb")) return NULL;
    assert(!strcmp(access,"wb"));
    opened=tmpfile(); assert(opened); return opened;
}
static int probe_close(FILE *stream)
{
    assert(stream==opened); assert(!fseek(stream,0,SEEK_SET));
    size_t n=fread(output,1,sizeof(output)-1,stream); output[n]=0;
    opened=NULL; return fclose(stream);
}
#define fopen probe_open
#define fclose probe_close
#define main preview_main
#include "../../firmware/tools/nativeinstallpreview/main.c"
#undef main
#undef fopen
#undef fclose
unsigned int sceKernelDevkitVersion(void) { return mode==4?0x06060010:0x06060110; }
int scePowerLock(int a) { assert(!a); return mode==1?-1:0; }
int scePowerUnlock(int a) { assert(!a); ++unlocks; return 0; }
int scePowerTick(int a) { assert(!a); return 0; }
int sceKernelDelayThread(unsigned int us)
{
    assert(us==1000000 || us==3000000 || us==20000);
    if(us==1000000) ++steps;
    if(us==3000000) assert(starts==1); /* No timed restart before a button. */
    return 0;
}
int proxNativeCompletionControl(int action)
{
    if(action==PROX_COMPLETION_FAIL) { ++fail_calls;return 0; }
    assert(steps==8);++notice_calls;
    if(action==PROX_COMPLETION_FINISH) return mode==5?-1:0;
    assert(action==PROX_COMPLETION_POLL);++polls;
    if(mode==6) return 0;
    if(mode==7 && polls>1) return -1;
    if(mode==8 && polls<=6001) return 1; /* Remain >120 seconds without reboot. */
    return polls==1?1:2;
}
int kuKernelLoadModule(const char *path,int flags,void *opt)
{
    assert(!strcmp(path,"ms0:/PSP/GAME/PROXUPDATE/native-preview-reboot.prx"));
    assert(!flags && !opt && steps==8 && polls>=2); ++loads; return mode==2?-1:17;
}
int sceKernelStartModule(int module,unsigned int size,void *args,int *status,void *opt)
{
    assert(module==17 && size==4 && *(int *)args==1 && !opt);
    ++starts; *status=mode==3?-1:0; return 0;
}
int main(void)
{
    char *args[]={"ms0:/PSP/GAME/PROXUPDATE/native-install-preview.prx",PROX_INSTALL_PREVIEW_ARGUMENT,"ms0:/PSP/GAME/PROXUPDATE"};
    for(mode=0;mode<9;++mode) {
        steps=loads=starts=unlocks=notice_calls=polls=fail_calls=0; output[0]=0;
        assert(preview_main(3,args)==-1 && !opened);
        if(mode==4) { assert(!steps && !loads && !starts && !unlocks && !output[0]); continue; }
        assert(strstr(output,"NO FLASH"));
        assert(unlocks==(mode!=1));
        if(mode==1) assert(!steps && !loads && !starts);
        else if(mode==5 || mode==6 || mode==7) assert(steps==8 && !loads && !starts && notice_calls==(mode==5?1:mode==6?251:3));
        else { assert(steps==8 && loads==1 && starts==(mode!=2) && notice_calls==(mode==8?6003:3)); }
        assert(fail_calls==1);
        if(mode==0 || mode==8) assert(strstr(output,"restart module accepted") && strstr(output,"PREVIEW RETURNED")==NULL);
        else assert(!strstr(output,"restart module accepted"));
    }
    assert(preview_main(2,args)==-1);
    args[1]="--prox-ota-v1"; assert(preview_main(3,args)==-1);
    puts("PASS: actual no-flash module validates arguments/FW, runs eight background steps and gates CFW restart; failures never announce install success; no renderer or flashing adapter");
}
