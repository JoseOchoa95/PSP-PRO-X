#define _GNU_SOURCE
/* Actual v2 launcher with isolated host files and mocked PSP calls. */
#include "native_target_shim.h"
#include "native_background.h"
#include "native_completion_store.h"
#include "prox_completion_preview.h"
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char root[]="ms0:/PROXDIAG/.dedicated-0000004d-00000001";
static const char engine[]="ms0:/PROXDIAG/.dedicated-0000004d-00000001/native/engine.prx";
static int scenario,armed,opened,bound,started,unloaded,faulted;
int prox_stopping(void) { return 0; }
void prox_log(const char *format,...) { assert(format); }
int sceUtilityGetSystemParamInt(int id,int *language)
{ assert(id==PSP_SYSTEMPARAM_ID_INT_LANGUAGE);*language=3;return 0; }
int prox_completion_read_store(FILE *file,unsigned int language,unsigned short *out,unsigned int capacity)
{
    assert(file && language==3 && capacity==PROX_COMPLETION_UNITS);
    if(scenario==2)return -1;
    out[0]=42;out[1]=0;return 2;
}
int prox_native_completion_prepare(const unsigned short *text,unsigned int units)
{ assert(units==2 && text[0]==42 && !text[1]);armed=1;return 0; }
int sceIoOpen(const char *path,int flags,int permissions)
{
    assert(armed && !strcmp(path,engine) && flags==(PSP_O_CREAT|PSP_O_WRONLY|PSP_O_EXCL));
    ++opened;return open(path,O_CREAT|O_WRONLY|O_EXCL,permissions);
}
int sceIoWrite(int fd,const void *bytes,unsigned int size) { return (int)write(fd,bytes,size); }
int sceIoClose(int fd) { return close(fd); }
int prox_file_check(const char *path,unsigned int size,const char *hash,int executable)
{
    unsigned char bytes[4];FILE *file=fopen(path,"rb");
    assert(!strcmp(path,engine) && size==sizeof(bytes) && !strcmp(hash,PROX_NATIVE_ENGINE_SHA256) && !executable);
    assert(file && fread(bytes,1,sizeof(bytes),file)==sizeof(bytes) && !fclose(file));
    assert(!memcmp(bytes,prox_native_engine,sizeof(bytes)));return 0;
}
int prox_native_install_exchange(int action,ProxNativeInstallMessage *packet)
{
    if(action==PROX_NI_ARM) {
        assert(armed && !strcmp(packet->job.root,root) && packet->worker.size==12 && packet->package.size==42);
        packet->lease=77;packet->generation=1;return scenario==3?-1:0;
    }
    assert(packet->lease==77 && packet->generation==1);
    if(action==PROX_NI_BIND) { assert(packet->module==17 && access(engine,F_OK));bound=1;return 0; }
    if(action==PROX_NI_STATUS) { assert(started && (scenario==4 || scenario==5));return scenario==4?-1:0; }
    assert(action==PROX_NI_FAIL && scenario==5 && unloaded);faulted=1;return 0;
}
int kuKernelLoadModule(const char *path,int flags,void *options)
{ assert(armed && opened && !flags && !options && !strcmp(path,engine));return 17; }
int sceKernelUnloadModule(int id) { assert(id==17 && scenario==5);unloaded=1;return 0; }
int prox_native_handoff_close(void) { assert(bound && !started);return 0; }
int sceKernelStartModule(int id,unsigned int used,void *arguments,int *status,void *options)
{
    const char *arg=arguments;
    assert(id==17 && bound && used>strlen(engine) && !strcmp(arg,engine) && !options);
    assert(!strcmp(arg+strlen(arg)+1,PROX_NI_ARGUMENT));
    started=1;*status=scenario>=4?-1:0;return scenario>=4?-1:0;
}
int sceKernelDelayThread(unsigned int delay) { (void)delay;assert(0);return -1; }

int main(void)
{
    char temp[]="/tmp/prox-dedicated-launch-XXXXXX", native[256];
    ProxDedicatedJob job={0};
    assert(mkdtemp(temp) && !chdir(temp));
    assert(!mkdir("ms0:",0700) && !mkdir("ms0:/PROXDIAG",0700) && !mkdir("flash0:",0700) && !mkdir("flash0:/kd",0700));
    assert(!mkdir(root,0700));snprintf(native,sizeof(native),"%s/native",root);assert(!mkdir(native,0700));
    strcpy(job.install.root,root);job.worker.size=12;job.package.size=42;
    for(scenario=0;scenario<6;++scenario) {
        armed=opened=bound=started=unloaded=faulted=0;
        if(scenario!=1) { FILE *resource=fopen(PROX_COMPLETION_STORE_PATH,"wb");assert(resource && !fclose(resource)); }
        int result=prox_native_background_launch_dedicated(&job);
        assert((result<0)==(scenario>=1 && scenario<=3));
        assert(armed==(scenario!=1 && scenario!=2) && opened==armed);
        assert(started==(scenario==0 || scenario>=4) && bound==started);
        assert(unloaded==(scenario==5) && faulted==unloaded);
        assert(access(engine,F_OK) && !access(root,F_OK));
        if(scenario!=1)assert(!unlink(PROX_COMPLETION_STORE_PATH));
    }
    puts("PASS: v2 launcher requires the internal completion store, removes the embedded engine copy, and preserves the stage after an uncertain start");
}
