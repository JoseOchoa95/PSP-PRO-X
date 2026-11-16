#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "prox_pbp_payload.h"
static void word(unsigned char *p,unsigned int n)
{ p[0]=n; p[1]=n>>8; p[2]=n>>16; p[3]=n>>24; }
int main(void)
{
    unsigned char h[40]; unsigned int start=0,length=0,i;
    memset(h,0,sizeof(h)); memcpy(h,"\0PBP",4); word(h+4,0x10000);
    for(i=0;i<7;++i) word(h+8+i*4,40);
    word(h+36,92);
    assert(!prox_pbp_payload(h,92,&start,&length) && start==40 && length==52);
    assert(prox_pbp_payload(h,91,&start,&length)<0);
    assert(prox_pbp_payload(h,16u*1024*1024+1,&start,&length)<0);
    h[1]='X'; assert(prox_pbp_payload(h,92,&start,&length)<0); h[1]='P';
    word(h+4,0); assert(prox_pbp_payload(h,92,&start,&length)<0); word(h+4,0x10000);
    word(h+12,39); assert(prox_pbp_payload(h,92,&start,&length)<0); word(h+12,40);
    word(h+36,93); assert(prox_pbp_payload(h,92,&start,&length)<0);
    word(h+36,91); assert(prox_pbp_payload(h,92,&start,&length)<0);
    word(h+36,40+4u*1024*1024);
    assert(!prox_pbp_payload(h,40+4u*1024*1024,&start,&length));
    word(h+36,41+4u*1024*1024);
    assert(prox_pbp_payload(h,41+4u*1024*1024,&start,&length)<0);
    puts("PASS: local PBP payload header/offset/file/allocation bounds"); return 0;
}
