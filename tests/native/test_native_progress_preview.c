#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_offer_preview.h"
#include "native_preview_stream.h"
static int owner=7,locked,opens,closes,cancel_after,delays;
int prox_preview_test_bytes(const void *p,unsigned int n) { return p && n && n<=32768; }
int sceKernelGetThreadId(void) { assert(!locked); return owner; }
unsigned int pspSdkDisableInterrupts(void) { assert(!locked); locked=1; return 12; }
void pspSdkEnableInterrupts(unsigned int mask) { assert(locked && mask==12); locked=0; }
int sceKernelDelayThread(unsigned int us)
{ assert(!locked && us==10000); if(cancel_after && ++delays==cancel_after) prox_native_preview_abort(); return 0; }
void prox_native_query_event(const char *s,int rc) { (void)s; (void)rc; assert(!locked); }
int sceIoOpen(const char *p,int f,int m)
{ (void)p; (void)m; assert(!locked && f==1); ++opens; return 3; }
int sceIoClose(int fd) { assert(!locked && fd==3); ++closes; return 0; }
int sceIoIoctl(int f,unsigned int c,void *i,int n,void *o,int z)
{ (void)f;(void)c;(void)i;(void)n;(void)o;(void)z; assert(0); return -1; }
int sceIoDevctl(const char *p,unsigned int c,void *i,int n,void *o,int z)
{ (void)p;(void)c;(void)i;(void)n;(void)o;(void)z; assert(0); return -1; }
int main(void)
{
    int (*open)(const char *,int,int)=prox_native_preview_io_function(0x109f50bc);
    int (*write)(int,const void *,int)=prox_native_preview_io_function(0x42ec03ac);
    int (*close)(int)=prox_native_preview_io_function(0x810c4bc3);
    int (*noop)(void)=prox_native_preview_io_function(0x06a70004);
    unsigned char buffer[32770]; unsigned int total=0; int fd,old,n;
    prox_native_preview_reset(); prox_native_preview_offer(1024*1024);
    assert(open("ms0:/PSP/GAME/OTHER/EBOOT.PBP",0x602,0777)<0);
    fd=open("ms0:/PSP/GAME/_UPDATE/EBOOT.PBP",0x602,0777); assert(fd>0 && !opens && !noop());
    assert(prox_native_preview_read(buffer,1)<0 && prox_native_preview_read(buffer,32767)<0);
    while(1) {
        memset(buffer,0xa5,sizeof(buffer));
        n=prox_native_preview_read(buffer+1,100000);
        if(n<0) break;
        assert(n>0 && n<=32768 && buffer[0]==0xa5 && buffer[n+1]==0xa5);
        assert(buffer[1]==0 && buffer[n]==0);
        owner=8; assert(write(fd,buffer+1,n)<0); owner=7;
        assert(write(fd,buffer+1,n)==n); total+=(unsigned int)n;
    }
    assert(total==1024*1024-32768 && !opens && !close(fd) && !closes);
    prox_native_preview_reset(); prox_native_preview_offer(1024*1024);
    fd=open("ms0:/PSP/GAME/_UPDATE12345678901234567890/EBOOT.PBP",0x602,0777);
    assert(fd>0); old=fd; cancel_after=2; delays=0;
    assert(prox_native_preview_read(buffer,32768)<0 && !close(fd));
    prox_native_preview_reset(); prox_native_preview_offer(1024*1024); cancel_after=0;
    fd=open("ef0:/PSP/GAME/UPDATE/EBOOT.PBP",0x602,0777); assert(fd>0 && fd!=old);
    assert(!close(old) && prox_native_preview_read(buffer,32768)>0);
    assert(!close(fd));
    assert(open("ms0:/PSP/GAME/_UPDATE1/EBOOT.PBP",0x602,0777)<0);
    assert(open("read-only",1,0)==3 && !close(3) && opens==1 && closes==1 && !locked);
    puts("PASS: production virtual file/read pacing/cancel, no real update writes, bounded caller copies and forced incomplete transfer"); return 0;
}
