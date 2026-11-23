/* Real retained signer -> production checked decoder -> production extractor.
 * Synthetic PRO bytes only; no PSP loader, network, target code or flash. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_signed.h"
#include "payload_fixture.h"
#define main inherited_signer_main
#include "../../firmware/contrib/PrxEncrypter/main.c"
#undef main
static unsigned int seen;
void prox_log(const char *format,...) { assert(format); }
static int collect(unsigned int id,const unsigned char *bytes,unsigned int size,void *unused)
{
    unsigned char expected[512];(void)unused;
    assert(id<11 && !(seen&(1u<<id)));image(expected,512,prox_native_payload_names[id]);
    assert(size==512 && !memcmp(bytes,expected,size));seen|=1u<<id;return 0;
}
static unsigned char *signed_fixture(unsigned int size,int wrong,unsigned int *length)
{
    unsigned char installer[5632],reboot[512],*launcher=malloc(size),*result;
    unsigned int i,a,b;FILE *f;char *args[]={"signer","/tmp/native-signed-input.prx","/tmp/native-signed-output.prx"};
    assert(launcher);image(installer,sizeof(installer),"PROUpdater");
    for(i=0;i<10;++i)image(installer+512+i*512,512,prox_native_payload_names[i]);
    image(reboot,sizeof(reboot),"fast_recovery");image(launcher,size,wrong?"NotLauncher":"PXELauncher");
    a=compressed(launcher+512,size-512,installer,sizeof(installer));b=compressed(launcher+512+a,size-512-a,reboot,sizeof(reboot));assert(a+b<size-512);
    f=fopen(args[1],"wb");assert(f);assert(fwrite(launcher,1,size,f)==size);assert(!fclose(f));free(launcher);
    assert(!inherited_signer_main(3,args));
    f=fopen(args[2],"rb");assert(f);assert(!fseek(f,0,SEEK_END));*length=(unsigned int)ftell(f);assert(!fseek(f,0,SEEK_SET));
    result=malloc(*length+1);assert(result && fread(result,1,*length,f)==*length && !fclose(f));return result;
}
int main(void)
{
    unsigned int i,n;unsigned char *bytes;ProxNativePayloadScan scan;
    unsigned int sizes[]={32768,409600,500000};
    for(i=0;i<3;++i) {
        bytes=signed_fixture(sizes[i],0,&n);memset(&scan,0,sizeof(scan));scan.sink=collect;seen=0;
        assert(!prox_native_signed_scan(&scan,bytes,n) && seen==0x7ff && !prox_native_payload_done(&scan));
        assert(prox_native_signed_scan(&scan,bytes,n)<0);free(bytes);
    }
    bytes=signed_fixture(32768,0,&n);
    for(i=0;i<5;++i) {
        memset(&scan,0,sizeof(scan));scan.sink=collect;seen=0;
        if(i==0)bytes[0x150+32]^=1;
        if(i==1)bytes[0x26]^=1;
        assert(prox_native_signed_scan(&scan,bytes,i==2?n-1:i==3?n+1:i==4?3u*1024u*1024u:n)<0 && !seen);
        if(i==0)bytes[0x150+32]^=1;
        if(i==1)bytes[0x26]^=1;
    }
    free(bytes);bytes=signed_fixture(32768,1,&n);memset(&scan,0,sizeof(scan));scan.sink=collect;seen=0;
    assert(prox_native_signed_scan(&scan,bytes,n)<0 && !seen);free(bytes);
    puts("PASS: real PRO signer/CMAC/KIRK/GZIP/ELF/payload pipeline extracts exact eleven synthetic images across signed envelopes; cipher tamper, unknown header, truncation/extra/oversize, wrong launcher and job reuse rejected; no PSP loader or flash");
    return 0;
}
