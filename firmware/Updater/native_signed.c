/* Bounded inverse of the retained PRO signer; never execute target code.
 * Release authenticity is mandatory at the caller (HTTPS + exact manifest
 * hashes). The fixed envelope and checked CMAC are format/integrity checks,
 * not an independent publisher signature. GPL-3.0-or-later. */
#include "native_signed.h"
#include "native_signed_templates.h"
#include "transport.h"
#include "../contrib/PrxEncrypter/types.h"
#include "../contrib/PrxEncrypter/kirk_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <zlib.h>
#define SIGNED_MAX (2u*1024u*1024u)
const char prox_signed_identity[]="PROX-NATIVE-DATA-ONLY-V1";
const char prox_signed_templates_identity[]=PROX_SIGNED_TEMPLATES_SOURCE_SHA256;
_Static_assert(sizeof(u32)==4,"Inherited KIRK word size");
int prox_signed_crypto_message(const char *format,...)
{
    char buffer[128];va_list args;int n;
    va_start(args,format);n=vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);
    prox_log("PRO payload crypto: %s",buffer);return n;
}
static unsigned int word(const unsigned char *p)
{ return p[0]|(unsigned int)p[1]<<8|(unsigned int)p[2]<<16|(unsigned int)p[3]<<24; }
static unsigned int half(const unsigned char *p) { return p[0]|(unsigned int)p[1]<<8; }
int prox_native_signed_scan(ProxNativePayloadScan *scan,const unsigned char *data,size_t size)
{
    const unsigned char *header=NULL;unsigned char *input=NULL,*plain=NULL,*image=NULL;
    unsigned int i,encoded,compressed,expanded,extent,ph,pn,count=0,total=0,last=0;char name[28];
    z_stream stream={0};int rc=-1,status;const char *step="supported signer envelope";
    if(!scan || !scan->sink || !data || size<0x160 || size>SIGNED_MAX || scan->found || scan->containers ||
       memcmp(data,"~PSP",4) || word(data+0x2c)!=size || half(data+6)!=1)goto done;
    for(i=0;i<3;++i)if(!memcmp(data,prox_signed_psp_templates[i],0x150))header=prox_signed_kirk_templates[i];
    if(!header)goto done;
    encoded=(unsigned int)size-0x150;compressed=word(header+0x70);expanded=word(data+0x28);
    if(!compressed || compressed>SIGNED_MAX-15 || ((compressed+15)&~15u)!=encoded ||
       word(header+0x60)!=1 || word(header+0x74)!=0x80 || !expanded || expanded>SIGNED_MAX)goto done;
    prox_log("%s; encrypted=%u expanded=%u; target is data only",prox_signed_identity,(unsigned int)size,expanded);
    step="KIRK CMAC/decrypt";
    input=malloc(encoded+0x110);plain=malloc(encoded);if(!input || !plain)goto done;
    memcpy(input,header,0x110);memcpy(input+0x110,data+0x150,encoded);
    if(kirk_init() || (status=kirk_CMD1(plain,input,(int)(encoded+0x110),1))!=0)goto done;
    free(input);input=NULL;
    step="bounded GZIP";
    image=malloc(expanded);if(!image)goto done;
    stream.next_in=plain;stream.avail_in=compressed;stream.next_out=image;stream.avail_out=expanded;
    if(inflateInit2(&stream,31)!=Z_OK)goto done;
    status=inflate(&stream,Z_FINISH);
    if(status!=Z_STREAM_END || stream.total_out!=expanded) { inflateEnd(&stream);goto done; }
    inflateEnd(&stream);free(plain);plain=NULL;
    step="launcher ELF identity/bounds";
    if(prox_native_image(image,expanded,name,&extent) || strcmp(name,"PXELauncher"))goto done;
    for(i=extent;i<expanded;++i)if(image[i])goto done;
    ph=word(image+28);pn=half(image+44);
    step="file-backed launcher payloads";
    for(i=0;i<pn;++i) {
        const unsigned char *p=image+ph+i*32;unsigned int start=word(p+4),length=word(p+16);
        if(word(p)!=1)continue;
        if(++count>4 || !length || start<last || start>extent || length>extent-start || total>SIGNED_MAX-length)goto done;
        last=start+length;total+=length;
        if(prox_native_payload_scan(scan,image+start,length))goto done;
    }
    if(!count || prox_native_payload_done(scan))goto done;
    prox_log("Signed target data decoded: CMAC/GZIP/ELF PASS; %u segments, ten PRO modules and matched rebooter; no target module load",count);
    rc=0;
done:
    free(input);free(plain);free(image);
    if(rc)prox_log("Signed target data decode failed at %s; no firmware writes",step);
    return rc;
}
