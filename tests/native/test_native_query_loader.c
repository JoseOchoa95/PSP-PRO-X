/* Actual loader: no-card vs missing file/internal storage, marker and owned I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pspkernel.h"
#include "prox_native_query.h"
#include "native_query_provider.h"
static int mode,inserted,internal,client,cached,failed,loads,starts,removed,closed_source,closed_target;
static unsigned int cause,position,written;
static int (*entry)(SceSize,void *);
static unsigned char pbp[296],copied[256];
int prox_native_query_cached_load(const char **path)
{
    if(mode!=7) return -1;
    assert(!inserted && !loads && !starts); ++loads;
    *path="ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP"; return 8;
}
static void put32(unsigned int offset,unsigned int value) { memcpy(pbp+offset,&value,4); }
void prox_native_query_event(const char *name,int result) { (void)name;(void)result; }
void prox_native_query_load_failed_reason(unsigned int reason) { ++failed; cause=reason; }
int MScmIsMediumInserted(void) { return inserted; }
void *sceKernelFindModuleByName(const char *name) { assert(!strcmp(name,"ProxUpdater")); return NULL; }
int sceIoGetstat(const char *name,SceIoStat *stat)
{
    assert(mode!=7); /* Successful RAM bootstrap must not inspect storage. */
    if(!strcmp(name,"ef0:/")) return internal?0:-1;
    if(!strcmp(name,"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP") && (client || cached)) {
        stat->st_size=sizeof(pbp); return 0;
    }
    return -1;
}
int sceIoOpen(const char *name,int flags,int access)
{
    if(flags==PSP_O_RDONLY) {
        assert(!strcmp(name,"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP") && !access);
        return inserted==0?-1:10;
    }
    assert(flags==(PSP_O_WRONLY|PSP_O_CREAT|PSP_O_EXCL) && access==0600 && strstr(name,"/.native-0000004d.prx"));
    return 11;
}
int sceIoRead(int fd,void *data,unsigned int size)
{
    assert(fd==10 && position<=sizeof(pbp) && size<=sizeof(pbp)-position);
    memcpy(data,pbp+position,size); position+=size; return (int)size;
}
int sceIoLseek(int fd,int offset,int mode_)
{ assert(fd==10 && mode_==0 && offset>=0 && offset<=(int)sizeof(pbp)); position=(unsigned int)offset; return offset; }
int sceIoWrite(int fd,const void *data,unsigned int size)
{
    assert(fd==11 && size<=sizeof(copied)-written); memcpy(copied+written,data,size); written+=size; return (int)size;
}
int sceIoClose(int fd) { if(fd==10) ++closed_source; else { assert(fd==11); ++closed_target; } return 0; }
int sceIoRemove(const char *name) { assert(strstr(name,"/.native-0000004d.prx")); ++removed; return 0; }
int sceKernelLoadModule(const char *name,int flags,SceKernelLMOption *option)
{
    assert(strstr(name,"/.native-") && !flags && option->size==sizeof(*option) && option->access==1 && option->position==1);
    assert(written==256 && !memcmp(copied,pbp+40,256) && closed_target==1); ++loads; return 8;
}
int sceKernelStartModule(int module,unsigned int size,void *arg,int *status,void *option)
{
    const char *first=arg,*second=first+strlen(first)+1,*third=second+strlen(second)+1;
    assert(module==8 && !option && size==strlen(first)+1+strlen(second)+1+9);
    assert(!strcmp(second,"--prox-native-query-v3") && !strcmp(third,"0000004d"));
    *status=0; ++starts; return 8;
}
int sceKernelUnloadModule(int module) { (void)module; assert(0); return -1; }
int sceKernelExitDeleteThread(int rc) { assert(!rc); return 0; }
int sceKernelCreateThread(const char *name,int (*callback)(SceSize,void *),int priority,int stack,int attr,void *option)
{ assert(strstr(name,"native loader") && priority==0x18 && stack==24576 && !attr && !option); entry=callback; return 3; }
int sceKernelStartThread(int id,unsigned int size,void *arg) { assert(id==3 && !size && !arg); entry(0,NULL); return 0; }
int sceKernelDeleteThread(int id) { (void)id; assert(0); return -1; }
int main(void)
{
    for(mode=0;mode<8;++mode) {
        unsigned int i;
        inserted=1; internal=client=cached=failed=loads=starts=removed=closed_source=closed_target=0;
        position=written=cause=0;
        memset(pbp,0,sizeof(pbp)); memcpy(pbp,"\0PBP",4); put32(4,0x10000);
        for(i=0;i<7;++i) put32(8+4*i,40);
        put32(36,sizeof(pbp)); memcpy(pbp+40,"\177ELF",4); pbp[44]=pbp[45]=1; pbp[58]=8;
        memcpy(pbp+120,mode==5?"PROX-NATIVE-QUERY-V2":"PROX-NATIVE-QUERY-V3",20);
        if(mode==0) inserted=0; /* No card, no internal storage. */
        if(mode==2) inserted=-1; /* Unknown hardware status must not claim no card. */
        if(mode==3) { inserted=0; internal=1; } /* Go: missing internal client is not missing MS. */
        if(mode==4) { inserted=0; cached=1; } /* Card removed after cached stat. */
        if(mode>=5) client=1;
        if(mode==7) inserted=0;
        assert(!prox_native_query_start(77) && !prox_native_query_is_loading());
        if(mode==7) assert(!failed && loads==1 && starts==1 && !removed && !closed_source && !closed_target);
        else if(mode==6) assert(!failed && loads==1 && starts==1 && removed==1 && closed_source==1 && closed_target==1);
        else {
            assert(failed==1 && !loads && !starts);
            assert(cause==((mode==0 || mode==4)?PROX_NQ_CAUSE_STORAGE:PROX_NQ_CAUSE_CLIENT));
            if(mode==5) assert(removed==1 && closed_source==1 && closed_target==1);
            else assert(!removed);
        }
    }
    puts("PASS: actual loader distinguishes no card/missing client/internal storage/unknown presence, handles removal, rejects v2 marker and copies only owned v3 user payload");
}
