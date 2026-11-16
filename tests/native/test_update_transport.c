#define _POSIX_C_SOURCE 200809L
#include "update_transport_shim.h"
#include "transport.h"
#include "network_session.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
static int port, stopping, cancel_body,dns_calls,tcp_calls,dns_failures,tcp_failures;
static int resolver_live,resolvers_created,resolvers_deleted,socket_live,sockets_created,sockets_closed;
static int link_state=4,disconnects,reconnects;
/* A real borrowed-session adapter is linked below the HTTPS retry loop.
 * Any shared stack mutation is a test failure, including during teardown. */
int sceNetApctlGetState(int *state)
{ assert(!resolver_live && !socket_live); *state=link_state; return 0; }
int proxUtilityGetLatestProfile(int *p) { *p=7; return 0; }
int sceUtilityCheckNetParam(int p) { assert(p==7); return 0; }
int sceNetApctlDisconnect(void)
{ assert(!resolver_live && !socket_live && link_state==4); link_state=0; ++disconnects; return 0; }
int sceNetApctlConnect(int p)
{ assert(!resolver_live && !socket_live && link_state==0 && p==7); link_state=4; ++reconnects; return 0; }
#define REJECT0(name) int name(void) { assert(!"shared XMB stack mutation"); return -1; }
#define REJECT1(name) int name(int a) { (void)a; assert(!"shared XMB stack mutation"); return -1; }
REJECT0(sceNetResolverInit) REJECT0(sceNetResolverTerm)
REJECT0(sceNetInetInit) REJECT0(sceNetInetTerm)
REJECT0(sceNetApctlTerm) REJECT0(sceNetTerm)
REJECT1(sceUtilityLoadNetModule) REJECT1(sceUtilityUnloadNetModule)
int sceNetApctlInit(int a,int b)
{ (void)a; (void)b; assert(!"shared XMB stack mutation"); return -1; }
int sceNetInit(int a,int b,int c,int d,int e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; assert(!"shared XMB stack mutation"); return -1; }
uint64_t sceKernelGetSystemTimeWide(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
unsigned int sceKernelGetSystemTimeLow(void) { return (unsigned int)sceKernelGetSystemTimeWide(); }
int sceKernelDelayThread(unsigned int n) { struct timespec t={0,n*1000}; return nanosleep(&t,NULL); }
int sceNetResolverCreate(int *r, void *b, int s)
{ (void)b; (void)s; assert(!resolver_live && !socket_live); *r=++resolvers_created; resolver_live=*r; return 0; }
int sceNetResolverDelete(int r)
{ assert(r==resolver_live); ++resolvers_deleted; resolver_live=0; return 0; }
int sceNetResolverStartNtoA(int r, const char *h, struct in_addr *a, unsigned int t, int n)
{ (void)r; (void)t; (void)n; ++dns_calls; if (dns_calls <= dns_failures) return -1;
  assert(!strcmp(h,"api.github.com") || !strcmp(h,"github.com")); a->s_addr=htonl(INADDR_LOOPBACK); return 0; }
int sceNetInetSocket(int a, int b, int c)
{ int s; assert(!resolver_live && !socket_live); s=socket(a,b,c); if(s>=0) { socket_live=1; ++sockets_created; } return s; }
int sceNetInetClose(int s)
{ int rc; assert(socket_live); rc=close(s); if(!rc) { socket_live=0; ++sockets_closed; } return rc; }
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
    if(getenv("PROX_TEST_RESIDENT_CA")) {
        static unsigned char trust[512*1024+1]; size_t n; FILE *ca=fopen(argv[3],"rb");
        assert(ca); n=fread(trust,1,sizeof(trust)-1,ca); assert(n && !ferror(ca) && feof(ca)); fclose(ca);
        trust[n]=0;
        assert(prox_transport_set_trust(NULL,n+1)<0 && prox_transport_set_trust(trust,0)<0);
        assert(!prox_transport_set_trust(trust,n+1) && prox_transport_set_trust(trust,n+1)<0);
        argv[3]="/nonexistent-removable-card/cacert.pem";
        puts("RESIDENT CA: immutable memory; filesystem path unavailable");
    }
    assert(!prox_network_borrow());
    if (cancel_body==2) stopping=1;
    rc=prox_https(argv[2],argv[3],(size_t)atoi(argv[4]),sink,&count,&response);
    assert(!resolver_live && !socket_live && !prox_network_end());
    printf("RESULT rc=%d status=%d bytes=%zu sink=%zu\n",rc,response.status,response.bytes,count);
    printf("CONNECT dns=%d tcp=%d\n",dns_calls,tcp_calls);
    printf("OWNED resolvers=%d/%d sockets=%d/%d\n",resolvers_created,resolvers_deleted,sockets_created,sockets_closed);
    printf("LINK disconnects=%d reconnects=%d state=%d\n",disconnects,reconnects,link_state);
    return rc ? 1 : 0;
}
