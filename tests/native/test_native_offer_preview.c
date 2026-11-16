#include <assert.h>
#include <stdio.h>
#include "native_offer_preview.h"
static int calls,events;
void prox_native_query_event(const char *text,int rc) { (void)text; assert(rc<0); ++events; }
int sceIoOpen(const char *path,int flags,int mode)
{ (void)path; (void)mode; assert(flags==1); ++calls; return 7; }
int sceIoIoctl(int fd,unsigned int cmd,void *in,int size,void *out,int length)
{ assert(fd==7 && (cmd==0x02125008 || cmd==0x02125009) && !in && !size && out && length==4); ++calls; return 0; }
int sceIoDevctl(const char *dev,unsigned int cmd,void *in,int size,void *out,int length)
{ (void)dev; assert(cmd==0x02125802 && !in && !size && out && length==4); ++calls; return 0; }
int main(void)
{
    int (*open)(const char *,int,int)=prox_native_preview_io_function(0x109f50bc);
    int (*deny)(int,const void *,int)=prox_native_preview_io_function(0x42ec03ac);
    int (*ioctl)(int,unsigned int,void *,int,void *,int)=prox_native_preview_io_function(0x63632449);
    int (*devctl)(const char *,unsigned int,void *,int,void *,int)=prox_native_preview_io_function(0x54f5fb11);
    int out=0,i;
    assert(open("read",1,0)==7 && calls==1);
    for(i=0;i<0x1000;++i) if(i!=1) assert(open("ignored",i,0777)<0);
    assert(calls==1 && deny(3,&out,4)<0 && events>4000);
    assert(!ioctl(7,0x02125008,0,0,&out,4));
    assert(!ioctl(7,0x02125009,0,0,&out,4));
    assert(ioctl(7,0,0,0,&out,4)<0 && ioctl(7,0x02125008,&out,4,&out,4)<0);
    assert(!devctl("msstor0:",0x02125802,0,0,&out,4));
    assert(devctl("ignored",0,0,0,&out,4)<0 && devctl("ignored",0x02125802,0,0,0,0)<0);
    assert(calls==4 && !prox_native_preview_io_known(0x12345678));
    assert(prox_native_preview_io_known(0x6a638d83) && !prox_native_preview_io_function(0x6a638d83));
    puts("PASS: offer preview denies native opens except read-only, mutations and unknown ioctl/devctl commands"); return 0;
}
