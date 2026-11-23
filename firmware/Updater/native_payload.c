/* Bounded data extraction. Never start the downloaded launcher or installer.
 * The caller authenticates its complete PBP and confines output paths. */
#include "native_payload.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#define IMAGE_MAX (1024u*1024u)
const char *const prox_native_payload_names[PROX_NI_PARTS]={
    "SystemControl","VshCtrl","PROGalaxyController","stargate","pspMarch33_Driver",
    "PRO_Inferno_Driver","pspUsbDev_Driver","PROPopcornManager","VshCtrlSatelite","Recovery","fast_recovery"
};
static unsigned int word(const unsigned char *p)
{ return p[0]|(unsigned int)p[1]<<8|(unsigned int)p[2]<<16|(unsigned int)p[3]<<24; }
static unsigned int half(const unsigned char *p) { return p[0]|(unsigned int)p[1]<<8; }
static int span(size_t size,unsigned int p,unsigned int n) { return n<=size && p<=size-n; }
static int name(const unsigned char *p,char out[28])
{
    const unsigned char *end=memchr(p,0,28);size_t n;
    if(!end || end==p) return -1;
    n=(size_t)(end-p);memcpy(out,p,n);out[n]=0;return 0;
}
int prox_native_image(const unsigned char *data,size_t size,char out[28],unsigned int *extent)
{
    unsigned int ph,sh,pn,sn,ss,i,total,strings,strings_size,info=0;
    if(!data || !out || !extent || size<52) return -1;
    if(!memcmp(data,"~PSP",4)) {
        total=word(data+0x2c);
        if(size<0x150 || total<0x150 || total>IMAGE_MAX || total>size || name(data+10,out)) return -1;
        *extent=total;return 0;
    }
    if(memcmp(data,"\177ELF",4) || data[4]!=1 || data[5]!=1 || data[6]!=1 ||
       half(data+16)!=0xffa0 || half(data+18)!=8 || half(data+40)!=52 ||
       half(data+42)!=32 || half(data+46)!=40) return -1;
    ph=word(data+28);sh=word(data+32);pn=half(data+44);sn=half(data+48);ss=half(data+50);
    if(!pn || pn>8 || !sn || sn>128 || ss>=sn || !span(size,ph,pn*32) || !span(size,sh,sn*40)) return -1;
    total=sh+sn*40;if(total<ph+pn*32) total=ph+pn*32;
    strings=word(data+sh+ss*40+16);strings_size=word(data+sh+ss*40+20);
    if(!strings_size || strings_size>16384 || !span(size,strings,strings_size)) return -1;
    for(i=0;i<pn;++i) {
        const unsigned char *p=data+ph+i*32;unsigned int offset=word(p+4),length=word(p+16);
        if(!span(size,offset,length)) return -1;
        if(offset+length>total) total=offset+length;
    }
    for(i=0;i<sn;++i) {
        const unsigned char *p=data+sh+i*40;
        unsigned int key=word(p),type=word(p+4),offset=word(p+16),length=word(p+20);
        if(key>=strings_size || !memchr(data+strings+key,0,strings_size-key)) return -1;
        if(type!=8) {
            if(!span(size,offset,length)) return -1;
            if(offset+length>total) total=offset+length;
        }
        if(!strcmp((const char *)data+strings+key,".rodata.sceModuleInfo")) {
            if(info || length<52 || !span(size,offset,length) || name(data+offset+4,out)) return -1;
            info=1;
        }
    }
    if(!info || total>IMAGE_MAX) return -1;
    *extent=total;return 0;
}
static int modules(ProxNativePayloadScan *scan,const unsigned char *data,unsigned int size)
{
    unsigned int ph=word(data+28),pn=half(data+44),i;
    /* Only file-backed PT_LOAD data can be embedded installer arrays. */
    for(i=0;i<pn;++i) {
        const unsigned char *p=data+ph+i*32;unsigned int offset=word(p+4),length=word(p+16),j;
        if(word(p)!=1 || !span(size,offset,length)) continue;
        for(j=0;j+52<=length;++j) {
            char module[28];unsigned int n,id,position=offset+j;
            if(memcmp(data+position,"~PSP",4) && memcmp(data+position,"\177ELF",4)) continue;
            if(prox_native_image(data+position,length-j,module,&n)) continue;
            for(id=0;id+1<PROX_NI_PARTS;++id) if(!strcmp(module,prox_native_payload_names[id])) break;
            if(id+1==PROX_NI_PARTS) continue;
            if((scan->found&(1u<<id)) || scan->sink(id,data+position,n,scan->context)) return -1;
            scan->found|=1u<<id;j+=n-1;
        }
    }
    return 0;
}
int prox_native_payload_scan(ProxNativePayloadScan *scan,const unsigned char *data,size_t size)
{
    size_t i;
    if(!scan || !scan->sink || !data || size>4u*1024u*1024u) return -1;
    for(i=0;i+10<=size;++i) {
        unsigned char *output;z_stream stream;char module[28];unsigned int extent;int rc;
        if(memcmp(data+i,"\037\213\010\000",4)) continue;
        if(++scan->candidates>32 || scan->inflated>=4u*IMAGE_MAX) return -1;
        output=malloc(IMAGE_MAX);if(!output) return -1;
        memset(&stream,0,sizeof(stream));stream.next_in=(Bytef *)data+i;stream.avail_in=(uInt)(size-i);
        stream.next_out=output;stream.avail_out=IMAGE_MAX;
        rc=inflateInit2(&stream,31);if(rc!=Z_OK) { free(output);return -1; }
        rc=inflate(&stream,Z_FINISH);scan->inflated+=stream.total_out;
        if(rc==Z_STREAM_END && !prox_native_image(output,stream.total_out,module,&extent) && extent==stream.total_out) {
            if(!strcmp(module,"PROUpdater")) {
                if(scan->containers&1u || modules(scan,output,extent)) rc=Z_DATA_ERROR;
                else scan->containers|=1u;
            } else if(!strcmp(module,"fast_recovery")) {
                if(scan->containers&2u || scan->sink(10,output,extent,scan->context)) rc=Z_DATA_ERROR;
                else { scan->containers|=2u;scan->found|=1u<<10; }
            }
        }
        if(rc==Z_STREAM_END && stream.total_in) i+=stream.total_in-1;
        inflateEnd(&stream);free(output);
        if(rc==Z_DATA_ERROR && (scan->containers || scan->found)) return -1;
    }
    return 0;
}
int prox_native_payload_done(const ProxNativePayloadScan *scan)
{ return scan && scan->containers==3u && scan->found==(1u<<PROX_NI_PARTS)-1?0:-1; }
