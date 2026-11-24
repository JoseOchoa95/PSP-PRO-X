#define _GNU_SOURCE
/* Actual v2 launcher with isolated host files and mocked PSP calls. */
#include "native_target_shim.h"
#include "native_background.h"
#include "native_completion_builtin.h"
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
{
    assert(id==PSP_SYSTEMPARAM_ID_INT_LANGUAGE);
    *language=scenario==2 ? 12 : 3;
    return scenario==1 ? -1 : 0;
}
int prox_native_completion_prepare(const unsigned short *text,unsigned int units)
{
    static const unsigned short expected[]=u"PRO-X se actualizó correctamente.";
    assert(units==sizeof(expected)/sizeof(expected[0]) && !memcmp(text,expected,sizeof(expected)));
    armed=1;
    return 0;
}
FILE *__real_fopen(const char *path,const char *mode);
FILE *__wrap_fopen(const char *path,const char *mode)
{
    assert(!strcmp(path,engine) && !strcmp(mode,"rb"));
    return __real_fopen(path,mode);
}
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
    int failed=scenario==4 || scenario==5;
    started=1;*status=failed?-1:0;return failed?-1:0;
}
int sceKernelDelayThread(unsigned int delay) { (void)delay;assert(0);return -1; }

static void check_languages(void)
{
    unsigned short message[PROX_COMPLETION_UNITS],copy[PROX_COMPLETION_UNITS];
    ProxCompletionState state={0};
    /* The first localized word/code unit follows the shared PRO-X name. */
    const unsigned short first[]={0x306e,'u','a','s','w',0x00e8,'i','f',0x0443,0xc5c5,0x5df2,0x5df2};
    for(unsigned int language=0;language<sizeof(first)/sizeof(first[0]);++language) {
        int units=prox_completion_builtin_text(language,message,PROX_COMPLETION_UNITS);
        assert(units>7 && (unsigned int)units<=PROX_COMPLETION_UNITS);
        assert(!memcmp(message,u"PRO-X",5*sizeof(*message)));
        assert(message[language==0 ? 5 : 6]==first[language] && !message[units-1]);
        assert(!prox_completion_arm(&state,1,copy,message,(unsigned int)units) &&
               !memcmp(message,copy,(unsigned int)units*sizeof(*message)));
        assert(prox_completion_builtin_text(language,copy,(unsigned int)units-1)<0);
    }
    assert(prox_completion_builtin_text(12,message,PROX_COMPLETION_UNITS)<0);
    assert(prox_completion_builtin_text(0,NULL,PROX_COMPLETION_UNITS)<0);
    puts("PASS: all 12 built-in UTF-16 messages, language order, native buffer and capacity rejection");
}

int main(void)
{
    check_languages();
    char temp[]="/tmp/prox-dedicated-launch-XXXXXX", native[256];
    ProxDedicatedJob job={0};
    assert(mkdtemp(temp) && !chdir(temp));
    assert(!mkdir("ms0:",0700) && !mkdir("ms0:/PROXDIAG",0700) && !mkdir("flash0:",0700) && !mkdir("flash0:/kd",0700));
    assert(!mkdir(root,0700));snprintf(native,sizeof(native),"%s/native",root);assert(!mkdir(native,0700));
    strcpy(job.install.root,root);job.worker.size=12;job.package.size=42;
    for(scenario=0;scenario<7;++scenario) {
        armed=opened=bound=started=unloaded=faulted=0;
        if(scenario==6) {
            /* A stale store from the published 0.4.0 kit must not affect this updater. */
            int fd=open(PROX_COMPLETION_STORE_PATH,O_CREAT|O_WRONLY|O_EXCL,0600);
            assert(fd>=0 && !close(fd));
        }
        int result=prox_native_background_launch_dedicated(&job);
        assert((result<0)==(scenario>=1 && scenario<=3));
        assert(armed==(scenario!=1 && scenario!=2) && opened==armed);
        assert(started==(scenario==0 || scenario>=4) && bound==started);
        assert(unloaded==(scenario==5) && faulted==unloaded);
        assert(access(engine,F_OK) && !access(root,F_OK));
        if(scenario==6)assert(!unlink(PROX_COMPLETION_STORE_PATH));
    }
    puts("PASS: v2 launcher uses built-in completion with missing or stale resources and preserves the verified writer lifecycle");
}
