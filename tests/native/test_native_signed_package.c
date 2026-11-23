/* Host-only production decoder consumer. Fixed filenames in an owned fixture
 * directory; no PSP loader or firmware-writing capability. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "native_signed.h"
#include "prox_pbp_payload.h"
static const char *directory;
static unsigned int seen;
void prox_log(const char *format,...)
{ va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);fputc('\n',stderr); }
static int collect(unsigned int id,const unsigned char *bytes,unsigned int size,void *context)
{
    char path[256];FILE *file;int rc;(void)context;
    if(id>=11 || (seen&(1u<<id)) || snprintf(path,sizeof(path),"%s/%02u.prx",directory,id)>=(int)sizeof(path))return -1;
    file=fopen(path,"wb");if(!file)return -1;rc=fwrite(bytes,1,size,file)==size?0:-1;if(fclose(file))rc=-1;
    if(!rc)seen|=1u<<id;
    return rc;
}
int main(int argc,char **argv)
{
    FILE *file;unsigned char *bytes;size_t size;unsigned int offset,length;int rc;
    ProxNativePayloadScan scan={0};
    if(argc!=3 || strlen(argv[2])>200)return 1;
    directory=argv[2];bytes=malloc(2u*1024u*1024u+1);if(!bytes)return 1;
    file=fopen(argv[1],"rb");if(!file) { free(bytes);return 1; }
    size=fread(bytes,1,2u*1024u*1024u+1,file);rc=ferror(file);if(fclose(file))rc=1;
    if(rc || size>2u*1024u*1024u || prox_pbp_payload(bytes,(unsigned int)size,&offset,&length)) { free(bytes);return 1; }
    scan.sink=collect;rc=prox_native_signed_scan(&scan,bytes+offset,length);free(bytes);
    if(rc || seen!=0x7ff || prox_native_payload_done(&scan))return 1;
    puts("PASS: actual signed PBP decoded as data; exact eleven images extracted; NO PSP MODULE LOAD OR FLASH CAPABILITY");return 0;
}
