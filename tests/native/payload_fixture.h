/* Minimal synthetic relocatable module containers for data-only tests. */
#ifndef PROX_PAYLOAD_FIXTURE_H
#define PROX_PAYLOAD_FIXTURE_H
#include <assert.h>
#include <string.h>
#include <zlib.h>
static void w(unsigned char *p,unsigned int x) { p[0]=x;p[1]=x>>8;p[2]=x>>16;p[3]=x>>24; }
static void h(unsigned char *p,unsigned int x) { p[0]=x;p[1]=x>>8; }
static void image(unsigned char *p,unsigned int size,const char *name)
{
    memset(p,0,size);memcpy(p,"\177ELF\1\1\1",7);h(p+16,0xffa0);h(p+18,8);
    w(p+28,52);w(p+32,96);h(p+40,52);h(p+42,32);h(p+44,1);h(p+46,40);h(p+48,2);h(p+50,0);
    w(p+52,1);w(p+68,size);w(p+72,size);
    w(p+96+4,3);w(p+96+16,176);w(p+96+20,24);
    w(p+136,1);w(p+140,1);w(p+152,208);w(p+156,52);
    memcpy(p+177,".rodata.sceModuleInfo",21);strcpy((char *)p+212,name);
}
static unsigned int compressed(unsigned char *out,unsigned int cap,unsigned char *in,unsigned int size)
{
    z_stream z={0};unsigned int n;assert(deflateInit2(&z,6,Z_DEFLATED,31,8,Z_DEFAULT_STRATEGY)==Z_OK);
    z.next_in=in;z.avail_in=size;z.next_out=out;z.avail_out=cap;
    assert(deflate(&z,Z_FINISH)==Z_STREAM_END);n=(unsigned int)z.total_out;deflateEnd(&z);return n;
}
#endif
