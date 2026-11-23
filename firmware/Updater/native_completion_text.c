/* Bounded read-only uncompressed original RCO text lookup. GPL-3.0-or-later.
 * RCOMage's published RCO header/entry/index layout; no Sony code/text bundled. */
#include "native_completion_text.h"
#include <string.h>
static unsigned int u32(const unsigned char *p)
{ return p[0]|(unsigned int)p[1]<<8|(unsigned int)p[2]<<16|(unsigned int)p[3]<<24; }
static unsigned int u16(const unsigned char *p) { return p[0]|(unsigned int)p[1]<<8; }
static int read_at(FILE *file,unsigned int size,unsigned int offset,void *out,unsigned int count)
{
    return count>size || offset>size-count || fseek(file,(long)offset,SEEK_SET) || fread(out,1,count,file)!=count?-1:0;
}
int prox_completion_read_text(FILE *file,unsigned int language,unsigned short *out,unsigned int capacity)
{
    unsigned char header[164],entry[48],index[12],label[64],text[240];
    unsigned int size,table,text_start,text_size,label_start,label_size,count,position,i;
    long length;
    if(!file || !out || capacity<2 || capacity>120 || language>11 ||
       fseek(file,0,SEEK_END) || (length=ftell(file))<164 || length>1024*1024) return -1;
    size=(unsigned int)length;
    if(read_at(file,size,0,header,sizeof(header)) || memcmp(header,"\0PRF",4) || u32(header+4)!=0x71 || u32(header+12)) return -1;
    table=u32(header+0x18);text_start=u32(header+0x38);text_size=u32(header+0x3c);
    label_start=u32(header+0x40);label_size=u32(header+0x44);
    if(text_size>size || text_start>size-text_size || label_size>size || label_start>size-label_size ||
       read_at(file,size,table,entry,40) || u32(entry)!=0x300 || (count=u32(entry+16))>12 || !count || table>size-40) return -1;
    position=table+40;
    for(i=0;i<count;++i) {
        unsigned int language_id,entries,step,j;
        if(read_at(file,size,position,entry,sizeof(entry)) || u32(entry)!=0x301 || u32(entry+8)!=40 || u16(entry+42)!=1) return -1;
        language_id=u16(entry+40);entries=u32(entry+44);step=u32(entry+20);
        if(entries>256 || position>size-48 || entries>(size-position-48)/12) return -1;
        if(language_id==language) {
            unsigned int matches=0,result=0;
            for(j=0;j<entries;++j) {
                unsigned int offset,n,k,bytes;
                if(read_at(file,size,position+48+j*12,index,sizeof(index))) return -1;
                offset=u32(index);bytes=u32(index+4);n=u32(index+8);
                if(offset>=label_size) return -1;
                k=label_size-offset; if(k>sizeof(label)) k=sizeof(label);
                if(read_at(file,size,label_start+offset,label,k) || !memchr(label,0,k)) return -1;
                if(strcmp((char *)label,"msgupdater_updatecomplete_ms_1")) continue;
                if(matches++ || bytes<2 || (bytes&1) || bytes>text_size || n>text_size-bytes) return -1;
                k=bytes; if(k>2*capacity) k=2*capacity;
                if(read_at(file,size,text_start+n,text,k)) return -1;
                for(offset=0;offset<k/2;++offset) {
                    unsigned int ch=u16(text+offset*2);
                    if(!ch || ch=='\r' || ch=='\n') break;
                    if(offset+1>=capacity) return -1;
                    out[offset]=(unsigned short)ch;
                }
                if(!offset || offset==k/2) return -1;
                out[offset]=0;result=offset+1;
            }
            return matches==1?(int)result:-1;
        }
        if(i+1<count) {
            if(step<48+12*entries || step>size || position>size-step) return -1;
            position+=step;
        }
    }
    return -1;
}
