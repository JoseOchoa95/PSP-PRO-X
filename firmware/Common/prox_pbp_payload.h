/* Bounded local PBP DATA.PSP extraction; no endpoint/flash behavior. GPL-3.0-or-later. */
#ifndef PROX_PBP_PAYLOAD_H
#define PROX_PBP_PAYLOAD_H
static unsigned int prox_pbp_u32(const unsigned char *p)
{ return p[0]|(unsigned int)p[1]<<8|(unsigned int)p[2]<<16|(unsigned int)p[3]<<24; }
static int prox_pbp_payload(const unsigned char *h,unsigned int size,unsigned int *start,unsigned int *length)
{
    unsigned int previous=40,i,offset,end;
    if(size<92 || size>16u*1024u*1024u || h[0] || h[1]!='P'||h[2]!='B'||h[3]!='P' ||
       prox_pbp_u32(h+4)!=0x10000) return -1;
    for(i=0;i<8;++i) {
        unsigned int value=prox_pbp_u32(h+8+4*i);
        if(value<previous || value>size) return -1;
        previous=value;
    }
    offset=prox_pbp_u32(h+32); end=prox_pbp_u32(h+36);
    if(end-offset<52 || end-offset>4u*1024u*1024u) return -1;
    *start=offset; *length=end-offset; return 0;
}
#endif
