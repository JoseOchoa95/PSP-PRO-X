#include "prox_network.h"
#include "pspiofilemgr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MISSING ((int)0x80010002)
typedef struct { int exists, size; unsigned char bytes[32]; } File;
static File files[3];
static int fail_write, fail_close, fail_sync, fail_commit, fail_restore, fail_open;
static int index_of(const char *path)
{
    if (strstr(path,".tmp")) return 1;
    if (strstr(path,".bak")) return 2;
    return 0;
}
int sceIoOpen(const char *p,int flags,int mode)
{
    int i=index_of(p);
    if (fail_open) return -9;
    if (flags & PSP_O_CREAT) { files[i].exists=1; files[i].size=0; }
    return files[i].exists ? i : MISSING;
}
int sceIoRead(int fd,void *p,unsigned int n)
{
    if (n > files[fd].size) n=files[fd].size;
    memcpy(p,files[fd].bytes,n);
    return n;
}
int sceIoWrite(int fd,const void *p,unsigned int n)
{
    if (fail_write) --n;
    memcpy(files[fd].bytes,p,n); files[fd].size=n; return n;
}
int sceIoClose(int fd) { return fail_close ? -1 : 0; }
int sceIoSync(const char *p,int flags) { return fail_sync ? -1 : 0; }
int sceIoGetstat(const char *p,SceIoStat *s) { return files[index_of(p)].exists ? 0 : MISSING; }
int sceIoRemove(const char *p)
{
    int i=index_of(p);
    if (!files[i].exists) return MISSING;
    files[i].exists=0; return 0;
}
int sceIoRename(const char *p,const char *q)
{
    int i=index_of(p),j=index_of(q);
    if ((fail_commit && i==1) || (fail_restore && i==2)) return -1;
    if (!files[i].exists) return MISSING;
    assert(!files[j].exists);
    files[j]=files[i]; files[i].exists=0; return 0;
}
static void reset(void)
{
    memset(files,0,sizeof(files));
    fail_write=fail_close=fail_sync=fail_commit=fail_restore=fail_open=0;
}
static void disabled(void)
{
    int enabled;
    assert(prox_network_read(&enabled) == PROX_NETWORK_VALID);
    assert(!enabled);
}
int main(void)
{
    int enabled,i;
    unsigned char bytes[13];
    reset();
    assert(prox_network_read(&enabled)==PROX_NETWORK_MISSING && enabled);
    assert(!files[0].exists && !files[1].exists && !files[2].exists);
    assert(prox_network_save(0)==0); disabled();
    assert(prox_network_save(1)==0);
    assert(prox_network_read(&enabled)==PROX_NETWORK_VALID && enabled);
    prox_network_encode(bytes,0);
    for(i=0;i<12;++i) {
        unsigned char old=bytes[i];
        bytes[i]=0xff;
        assert(prox_network_decode(bytes,12,&enabled)<0 && enabled);
        bytes[i]=old;
    }
    assert(prox_network_decode(bytes,11,&enabled)<0 && enabled);
    assert(prox_network_decode(bytes,13,&enabled)<0 && enabled);
    reset(); assert(prox_network_save(0)==0);
    fail_write=1; assert(prox_network_save(1)<0); fail_write=0; disabled();
    fail_close=1; assert(prox_network_save(1)<0); fail_close=0; disabled();
    fail_sync=1; assert(prox_network_save(1)<0); fail_sync=0; disabled();
    fail_commit=1; assert(prox_network_save(1)<0); disabled();
    fail_restore=1; assert(prox_network_save(1)<0);
    assert(prox_network_read(&enabled)==PROX_NETWORK_RECOVERED && !enabled);
    fail_commit=fail_restore=0;
    assert(prox_network_save(1)==0);
    assert(prox_network_read(&enabled)==PROX_NETWORK_VALID && enabled);
    files[0].bytes[4]=2;
    assert(prox_network_read(&enabled)==PROX_NETWORK_INVALID && enabled);
    fail_open=1;
    assert(prox_network_read(&enabled)==PROX_NETWORK_IO_ERROR && enabled);
    puts("PASS: private config defaults, bounded parsing, save failures and recovery");
    return 0;
}
