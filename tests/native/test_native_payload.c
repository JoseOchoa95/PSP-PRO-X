/* Synthetic module data only. No PSP loader or flash capability. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "native_payload.h"
#include "payload_fixture.h"
static int count;
static int sink(unsigned int id,const unsigned char *data,unsigned int size,void *arg)
{
    char name[28];unsigned int extent;(void)arg;
    assert(id<11 && size==512 && !prox_native_image(data,size,name,&extent));
    assert(extent==size && !strcmp(name,prox_native_payload_names[id]));++count;return 0;
}
int main(void)
{
    unsigned char installer[5632],reboot[512],carrier[8192],small[512];unsigned int i,a,b,n;char name[28];
    ProxNativePayloadScan scan={0};
    image(installer,sizeof(installer),"PROUpdater");
    for(i=0;i<10;++i) image(installer+512+i*512,512,prox_native_payload_names[i]);
    image(reboot,sizeof(reboot),"fast_recovery");
    a=compressed(carrier,sizeof(carrier),installer,sizeof(installer));
    b=compressed(carrier+a,sizeof(carrier)-a,reboot,sizeof(reboot));
    scan.sink=sink;assert(!prox_native_payload_scan(&scan,carrier,a+b));assert(!prox_native_payload_done(&scan) && count==11);
    assert(prox_native_payload_scan(&scan,carrier+a,b)<0); /* Duplicate containers. */
    memset(&scan,0,sizeof(scan));scan.sink=sink;
    assert(!prox_native_payload_scan(&scan,carrier,a+b-4) && prox_native_payload_done(&scan)<0);
    image(small,sizeof(small),"Recovery");assert(!prox_native_image(small,sizeof(small),name,&n) && n==512);
    w(small+32,0xfffffff0);assert(prox_native_image(small,sizeof(small),name,&n)<0);
    image(small,sizeof(small),"Recovery");w(small+152,0xfffffff0);assert(prox_native_image(small,sizeof(small),name,&n)<0);
    memset(small,0,sizeof(small));memcpy(small,"~PSP",4);strcpy((char *)small+10,"VshCtrl");w(small+0x2c,512);
    assert(!prox_native_image(small,sizeof(small),name,&n) && n==512 && !strcmp(name,"VshCtrl"));
    w(small+0x2c,513);assert(prox_native_image(small,sizeof(small),name,&n)<0);
    memset(&scan,0,sizeof(scan));scan.sink=sink;scan.candidates=32;
    assert(prox_native_payload_scan(&scan,carrier,a+b)<0);
    puts("PASS: data-only PRO payload extractor finds exact 10 modules/target rebooter, rejects duplicate/truncated/bounded ELF/PSP/gzip input; no target code execution or flash");
}
