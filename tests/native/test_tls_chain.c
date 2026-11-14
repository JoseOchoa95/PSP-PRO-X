/* Parse the pinned CA corpus and an optional captured public certificate chain.
 * No TLS/network, endpoint override, private key or PSP binding in this test. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_time.h"
mbedtls_ms_time_t mbedtls_ms_time(void) { return (mbedtls_ms_time_t)time(NULL)*1000; }
static int load(mbedtls_x509_crt *chain, const char *path)
{
    FILE *file = fopen(path,"rb");
    unsigned char *buffer;
    long size;
    int result;
    if (!file || fseek(file,0,SEEK_END) || (size=ftell(file))<1 || size>512*1024 || fseek(file,0,SEEK_SET)) return -1;
    buffer=malloc((size_t)size+1);
    if (!buffer || fread(buffer,1,(size_t)size,file)!=(size_t)size) return -1;
    fclose(file); buffer[size]=0;
    result=mbedtls_x509_crt_parse(chain,buffer,(size_t)size+1);
    free(buffer); return result;
}
int main(int argc, char **argv)
{
    mbedtls_x509_crt ca, chain;
    unsigned int flags=0;
    int parsed, certificates=0, rc=0;
    mbedtls_x509_crt *entry;
    if (argc!=2 && argc!=4) return 2;
    mbedtls_x509_crt_init(&ca); mbedtls_x509_crt_init(&chain);
    parsed=load(&ca,argv[1]);
    for(entry=&ca;entry && entry->raw.p;entry=entry->next) ++certificates;
    printf("CA skipped=%d parsed=%d\n",parsed,certificates);
    if (parsed!=0) rc=1;
    if (argc==4) {
        int peer=load(&chain,argv[2]);
        int verified=mbedtls_x509_crt_verify(&chain,&ca,NULL,argv[3],&flags,NULL,NULL);
        char info[512] = {0};
        mbedtls_x509_crt_verify_info(info,sizeof(info),"",flags);
        printf("CHAIN parse=%d verify=%d flags=0x%08x %s\n",peer,verified,flags,info);
        if (peer || verified || flags) rc=1;
    }
    mbedtls_x509_crt_free(&chain); mbedtls_x509_crt_free(&ca);
    return rc;
}
