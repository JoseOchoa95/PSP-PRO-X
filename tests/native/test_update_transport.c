#define _POSIX_C_SOURCE 200809L
#include "update_transport_shim.h"
#include "transport.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
static int port, stopping, cancel_body,dns_calls,tcp_calls,dns_failures,tcp_failures;
uint64_t sceKernelGetSystemTimeWide(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
unsigned int sceKernelGetSystemTimeLow(void) { return (unsigned int)sceKernelGetSystemTimeWide(); }
int sceKernelDelayThread(unsigned int n) { struct timespec t={0,n*1000}; return nanosleep(&t,NULL); }
int sceNetResolverCreate(int *r, void *b, int s) { (void)b; (void)s; *r=1; return 0; }
int sceNetResolverDelete(int r) { (void)r; return 0; }
int sceNetResolverStartNtoA(int r, const char *h, struct in_addr *a, unsigned int t, int n)
{ (void)r; (void)t; (void)n; ++dns_calls; if (dns_calls <= dns_failures) return -1;
  assert(!strcmp(h,"api.github.com") || !strcmp(h,"github.com")); a->s_addr=htonl(INADDR_LOOPBACK); return 0; }
int sceNetInetSocket(int a, int b, int c) { return socket(a,b,c); }
int sceNetInetSetsockopt(int s, int l, int n, const void *v, socklen_t z)
{ if (n==SO_NONBLOCK) return fcntl(s,F_SETFL,O_NONBLOCK); return setsockopt(s,l,n,v,z); }
int sceNetInetConnect(int s, const struct sockaddr *a, socklen_t n)
{ struct sockaddr_in b=*(const struct sockaddr_in *)a; ++tcp_calls;
  if (tcp_calls <= tcp_failures) { errno=ECONNREFUSED; return -1; }
  b.sin_port=htons((unsigned short)port); return connect(s,(void *)&b,n); }
int sceNetInetGetErrno(void) { return errno; }
int proxUpdateRandom(void *b, unsigned int n)
{ FILE *f=fopen("/dev/urandom","rb"); int ok=f && fread(b,1,n,f)==n; if(f) fclose(f); return ok ? 0 : -1; }
int prox_stopping(void) { return stopping; }
void prox_log(const char *format, ...)
{ va_list a; va_start(a,format); vprintf(format,a); puts(""); va_end(a); }
static int sink(const unsigned char *p, size_t n, void *ctx)
{ (void)p; *(size_t *)ctx+=n; if(cancel_body) stopping=1; return 0; }
int main(int argc, char **argv)
{
    ProxResponse response;
    size_t count=0;
    int rc;
    assert(argc==6);
    dns_failures=getenv("PROX_TEST_DNS_FAILURES") ? atoi(getenv("PROX_TEST_DNS_FAILURES")) : 0;
    tcp_failures=getenv("PROX_TEST_TCP_FAILURES") ? atoi(getenv("PROX_TEST_TCP_FAILURES")) : 0;
    port=atoi(argv[1]); cancel_body=atoi(argv[5]);
    if (cancel_body==2) stopping=1;
    rc=prox_https(argv[2],argv[3],(size_t)atoi(argv[4]),sink,&count,&response);
    printf("RESULT rc=%d status=%d bytes=%zu sink=%zu\n",rc,response.status,response.bytes,count);
    printf("CONNECT dns=%d tcp=%d\n",dns_calls,tcp_calls);
    return rc ? 1 : 0;
}
