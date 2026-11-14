/* PRO-X Network Diagnostic 1.0, GPL-3.0-or-later.
 * User-mode homebrew: reads existing profiles, connects and writes card logs.
 * No flash writes, profile edits, module patches or installer dependencies. */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <pspwlan.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <psphttp.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include "protocol.h"

/* The pinned SDK matches the firmware's 32-bit timeval layout. Reject a
 * toolchain with the incompatible newlib 64-bit seconds layout. */
_Static_assert(sizeof(struct timeval)==8,"PSP timeval ABI mismatch");
_Static_assert(offsetof(struct timeval,tv_usec)==4,"PSP timeval offset mismatch");

PSP_MODULE_INFO("ProxNetDiag",0,1,0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(64);
#ifndef NETDIAG_BUILD_ID
#define NETDIAG_BUILD_ID "unidentified"
#endif

static SceUID report = -1;
static char report_path[256];
static unsigned int epoch;
static int log_failed, cancelled, resolver_ready;
static volatile int home_requested;
static SceUID event_lock = -1;
typedef struct { int old_state, new_state, event, error; unsigned int time; } Event;
static Event events[64];
static int event_count, event_read;
static volatile int events_dropped;

static unsigned int elapsed(unsigned int start)
{ return (unsigned int)(sceKernelGetSystemTimeLow()-start); }

/* Only the main thread writes. Each complete line goes straight to the card. */
static void record(const char *format, ...)
{
    char line[448];
    va_list args;
    int n, offset=0;
    n=snprintf(line,sizeof(line),"[%8u ms] ",elapsed(epoch)/1000);
    va_start(args,format);
    vsnprintf(line+n,sizeof(line)-(size_t)n-2,format,args);
    va_end(args);
    n=(int)strlen(line); line[n++]='\n'; line[n]=0;
    pspDebugScreenPrintf("%s",line);
    while (report >= 0 && offset < n && !log_failed) {
        int written=sceIoWrite(report,line+offset,n-offset);
        if (written <= 0) {
            log_failed=1;
            pspDebugScreenPrintf("REPORT WRITE FAILED: 0x%08x\n",written);
        } else offset+=written;
    }
}

static void result(const char *stage, int rc)
{ record("%s rc=%d hex=0x%08x",stage,rc,(unsigned int)rc); }

static int stopping(void)
{
    SceCtrlData pad;
    int rc;
    memset(&pad,0,sizeof(pad));
    rc=sceCtrlPeekBufferPositive(&pad,1);
    if (rc < 0) { result("controller",rc); cancelled=1; }
    if (home_requested || (rc > 0 && (pad.Buttons & PSP_CTRL_CIRCLE))) cancelled=1;
    return cancelled || log_failed;
}

static int exit_callback(int a, int b, void *arg)
{ (void)a; (void)b; (void)arg; home_requested=1; return 0; }

static int callback_thread(SceSize args, void *argp)
{
    int cb;
    (void)args; (void)argp;
    cb=sceKernelCreateCallback("NETDIAG exit",exit_callback,NULL);
    if (cb >= 0) sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

static void ap_event(int old_state, int new_state, int event, int error, void *arg)
{
    (void)arg;
    /* Never perform file I/O or block the network's event thread. */
    if (sceKernelPollSema(event_lock,1) < 0) { ++events_dropped; return; }
    if (event_count < 64) {
        Event *e=&events[event_count++];
        e->old_state=old_state; e->new_state=new_state;
        e->event=event; e->error=error; e->time=elapsed(epoch)/1000;
    } else ++events_dropped;
    sceKernelSignalSema(event_lock,1);
}

static void drain_events(void)
{
    Event copy[64];
    int n=0, i;
    if (event_lock < 0 || sceKernelPollSema(event_lock,1) < 0) return;
    while (event_read < event_count) copy[n++]=events[event_read++];
    sceKernelSignalSema(event_lock,1);
    for (i=0;i<n;++i)
        record("ap_event at_ms=%u old=%d new=%d event=%d error=0x%08x",
               copy[i].time,copy[i].old_state,copy[i].new_state,
               copy[i].event,(unsigned int)copy[i].error);
}

static int open_report(void)
{
    const char *roots[]={"ms0:/PSP/GAME/NETDIAG","ef0:/PSP/GAME/NETDIAG"};
    unsigned int i, number;
    for (i=0;i<2;++i) {
        for (number=1;number<=999;++number) {
            SceIoStat stat;
            snprintf(report_path,sizeof(report_path),"%s/report-%03u.txt",roots[i],number);
            if (sceIoGetstat(report_path,&stat) >= 0) continue;
            report=sceIoOpen(report_path,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_EXCL,0666);
            if (report >= 0) return 1;
            break;
        }
    }
    return 0;
}

static int choose_profile(void)
{
    int ids[100], count=0, id, selected=0, redraw=1;
    unsigned int previous=~0u, start=sceKernelGetSystemTimeLow();
    for (id=1;id<=99;++id) if (sceUtilityCheckNetParam(id)==0) ids[count++]=id;
    record("saved_profile_count=%d (IDs 1..99 checked; names omitted from report)",count);
    if (!count) { record("STOP: create a connection profile in XMB first"); return -1; }
    while (!stopping() && elapsed(start)<120000000u) {
        SceCtrlData pad;
        unsigned int pressed;
        if (redraw) {
            netData name;
            char display[42];
            int i;
            memset(&name,0,sizeof(name)); memset(display,0,sizeof(display));
            if (sceUtilityGetNetParam(ids[selected],PSP_NETPARAM_NAME,&name)==0)
                for (i=0;i<40 && name.asString[i];++i)
                    display[i]=(name.asString[i]>=32 && name.asString[i]<127) ? name.asString[i] : '?';
            pspDebugScreenClear();
            pspDebugScreenPrintf("PRO-X Network Diagnostic 1.0\n\nActivate PRO-X before launching this app.\n\n");
            pspDebugScreenPrintf("Profile %d (%d/%d)\n%s\n\n",ids[selected],selected+1,count,display);
            pspDebugScreenPrintf("UP/DOWN: choose saved profile\nX: start tests    O/HOME: cancel\n\n");
            pspDebugScreenPrintf("Uses this profile without editing it.\nDNS: profile, gateway, Cloudflare and Quad9.\nHTTP: raw sockets and Sony HTTP library.\nSony test uses HTTP 1.0 and 1.1 templates.\n\n");
            pspDebugScreenPrintf("Private report includes local IP/gateway/DNS.\nNo passwords, SSID or MAC addresses are logged.\n");
            redraw=0;
        }
        memset(&pad,0,sizeof(pad));
        sceCtrlPeekBufferPositive(&pad,1);
        pressed=pad.Buttons & ~previous; previous=pad.Buttons;
        if (pressed & PSP_CTRL_UP) { selected=(selected+count-1)%count; redraw=1; }
        if (pressed & PSP_CTRL_DOWN) { selected=(selected+1)%count; redraw=1; }
        if (pressed & PSP_CTRL_CROSS) {
            pspDebugScreenClear();
            record("selected_profile_id=%d",ids[selected]);
            return ids[selected];
        }
        sceKernelDelayThread(20000);
    }
    cancelled=1;
    return -1;
}

static int connect_profile(int profile)
{
    int rc, state, previous=-1;
    unsigned int start=sceKernelGetSystemTimeLow();
    rc=sceNetApctlConnect(profile); result("apctl_connect",rc);
    if (rc < 0) return 0;
    while (!stopping() && elapsed(start)<45000000u) {
        drain_events();
        rc=sceNetApctlGetState(&state);
        if (rc < 0) { result("apctl_get_state",rc); return 0; }
        if (state != previous) { record("apctl_state=%d",state); previous=state; }
        if (state==PSP_NET_APCTL_STATE_GOT_IP) { record("IP_READY (not proof of Internet access)"); return 1; }
        sceKernelDelayThread(20000);
    }
    record("association_ip=%s limit_ms=45000",cancelled ? "CANCELLED" : "TIMEOUT");
    return 0;
}

static void info_address(int code, const char *label, char *destination)
{
    union SceNetApctlInfo info;
    int rc;
    memset(&info,0,sizeof(info));
    rc=sceNetApctlGetInfo(code,&info);
    if (rc < 0) { result(label,rc); if (destination) destination[0]=0; return; }
    /* All selected address members begin at offset zero in the SDK union. */
    info.ip[15]=0;
    record("%s=%s",label,info.ip);
    if (destination) memcpy(destination,info.ip,16);
}

static int nonblocking(int type)
{
    int s=sceNetInetSocket(AF_INET,type,0), one=1, rc;
    if (s < 0) { record("socket FAIL rc=%d errno=%d",s,sceNetInetGetErrno()); return -1; }
    if (s >= FD_SETSIZE) {
        record("socket FAIL descriptor=%d exceeds select capacity=%d",s,FD_SETSIZE);
        sceNetInetClose(s); return -1;
    }
    rc=sceNetInetSetsockopt(s,SOL_SOCKET,SO_NONBLOCK,&one,sizeof(one));
    if (rc < 0) {
        record("nonblocking FAIL rc=%d errno=%d",rc,sceNetInetGetErrno());
        sceNetInetClose(s); return -1;
    }
    return s;
}

/* 50ms select slices allow cancellation; no unbounded socket wait. */
static int ready(int s, int writing, unsigned int start, unsigned int limit)
{
    while (!stopping() && elapsed(start)<limit) {
        fd_set set;
        struct timeval wait={0,50000};
        int rc;
        FD_ZERO(&set); FD_SET(s,&set);
        rc=sceNetInetSelect(s+1,writing ? NULL : &set,writing ? &set : NULL,NULL,&wait);
        if (rc > 0) return 1;
        if (rc < 0) { record("select FAIL rc=%d errno=%d",rc,sceNetInetGetErrno()); return -1; }
        drain_events();
    }
    record("socket_wait=%s",cancelled ? "CANCELLED" : "TIMEOUT");
    return 0;
}

static void sock_address(struct sockaddr_in *out, const struct in_addr *ip, int port)
{
    memset(out,0,sizeof(*out)); out->sin_len=sizeof(*out);
    out->sin_family=AF_INET; out->sin_port=htons(port); out->sin_addr=*ip;
}

typedef struct {
    int rid, rc;
    const char *host;
    struct in_addr address;
    unsigned char scratch[1024];
} ResolverJob;
static ResolverJob job;

static int resolver_thread(SceSize size, void *arg)
{
    (void)size; (void)arg;
    job.rc=sceNetResolverStartNtoA(job.rid,job.host,&job.address,3,1);
    return 0;
}

/* The potentially blocking SDK resolver runs in its own thread. Never free its
 * scratch memory or unload networking while that thread could still use it. */
static int native_dns(const char *host, struct in_addr *out)
{
    SceUID thread;
    SceUInt wait;
    unsigned int start=sceKernelGetSystemTimeLow();
    int rc;
    if (!resolver_ready) { record("native_dns SKIP resolver unavailable host=%s",host); return 0; }
    memset(&job,0,sizeof(job)); job.host=host; job.rc=-1;
    rc=sceNetResolverCreate(&job.rid,job.scratch,sizeof(job.scratch));
    result("resolver_create",rc); if (rc < 0) return 0;
    thread=sceKernelCreateThread("NETDIAG DNS",resolver_thread,0x18,0x4000,PSP_THREAD_ATTR_USER,NULL);
    if (thread < 0) { result("resolver_thread_create",thread); sceNetResolverDelete(job.rid); return 0; }
    record("native_dns BEGIN host=%s SDK_timeout_s=3 retry=1 watchdog_ms=12000",host);
    rc=sceKernelStartThread(thread,0,NULL);
    if (rc < 0) { result("resolver_thread_start",rc); sceKernelDeleteThread(thread); sceNetResolverDelete(job.rid); return 0; }
    for (;;) {
        wait=50000;
        rc=sceKernelWaitThreadEnd(thread,&wait);
        if (rc==0) break;
        if (stopping() || elapsed(start)>=12000000u) {
            record("native_dns %s host=%s",cancelled ? "CANCELLED" : "TIMEOUT",host);
            result("resolver_stop",sceNetResolverStop(job.rid));
            wait=1000000;
            rc=sceKernelWaitThreadEnd(thread,&wait);
            if (rc < 0) {
                record("STOP resolver worker did not stop; return to XMB without unloading live libraries");
                if (report >= 0) { sceIoClose(report); report=-1; }
                sceKernelExitGame();
                sceKernelExitDeleteThread(0);
            }
            record("native_dns return_after_stop=%d hex=0x%08x (after cancellation; not original DNS failure)",
                   job.rc,(unsigned int)job.rc);
            sceKernelDeleteThread(thread); sceNetResolverDelete(job.rid);
            return 0;
        }
    }
    record("native_dns host=%s rc=%d hex=0x%08x duration_ms=%u",host,job.rc,
           (unsigned int)job.rc,elapsed(start)/1000);
    if (job.rc==0) { *out=job.address; record("native_dns address=%s",inet_ntoa(*out)); }
    sceKernelDeleteThread(thread); sceNetResolverDelete(job.rid);
    return job.rc==0;
}

static int direct_dns(const char *server, const char *host, struct in_addr *out)
{
    unsigned char query[272], reply[512], address[4];
    struct in_addr server_ip;
    struct sockaddr_in dest;
    unsigned short id=(unsigned short)sceKernelGetSystemTimeLow();
    unsigned int start=sceKernelGetSystemTimeLow();
    int s, rc, length, found=0, error, received;
    if (stopping() || !server[0] || !strcmp(server,"0.0.0.0")) return 0;
    if (!inet_aton(server,&server_ip)) { record("direct_dns SKIP invalid server address"); return 0; }
    record("direct_dns BEGIN server=%s host=%s limit_ms=3000",server,host);
    length=nd_dns_query(query,sizeof(query),id,host);
    s=nonblocking(SOCK_DGRAM); if (s < 0 || length < 0) return 0;
    sock_address(&dest,&server_ip,53);
    rc=sceNetInetConnect(s,(struct sockaddr *)&dest,sizeof(dest));
    if (rc < 0) { record("UDP connect FAIL rc=%d errno=%d",rc,sceNetInetGetErrno()); goto done; }
    rc=sceNetInetSend(s,query,length,0);
    if (rc != length) { record("UDP send FAIL rc=%d errno=%d",rc,sceNetInetGetErrno()); goto done; }
    record("direct_dns query_sent_bytes=%d server=%s",rc,server);
    while (ready(s,0,start,3000000u)>0) {
        received=sceNetInetRecv(s,reply,sizeof(reply),0);
        if (received < 0) {
            error=sceNetInetGetErrno();
            if (error==EAGAIN || error==EWOULDBLOCK) continue;
            record("UDP recv FAIL rc=%d errno=%d",received,error); break;
        }
        rc=nd_dns_answer(reply,(size_t)received,id,host,address,&found);
        record("direct_dns reply server=%s parse_rc=%d has_A=%d bytes=%d duration_ms=%u",
               server,rc,found,received,elapsed(start)/1000);
        if (rc==0 && found) { memcpy(&out->s_addr,address,4); record("direct_dns address=%s",inet_ntoa(*out)); }
        else found=0;
        break;
    }
done:
    sceNetInetClose(s);
    if (!found) record("direct_dns NO_ADDRESS server=%s (not proof of no Internet)",server);
    return found;
}

static int tcp_connect(struct in_addr ip, int port, const char *scope)
{
    int s, rc, error;
    socklen_t option_size=sizeof(error);
    struct sockaddr_in dest;
    unsigned int start=sceKernelGetSystemTimeLow();
    if (stopping()) return -1;
    record("tcp BEGIN scope=%s ip=%s port=%d limit_ms=5000",scope,inet_ntoa(ip),port);
    s=nonblocking(SOCK_STREAM); if (s < 0) return -1;
    sock_address(&dest,&ip,port);
    rc=sceNetInetConnect(s,(struct sockaddr *)&dest,sizeof(dest));
    if (rc < 0) {
        error=sceNetInetGetErrno();
        record("tcp_connect initial_rc=%d errno=%d",rc,error);
        if (error != EINPROGRESS && error != EAGAIN && error != EWOULDBLOCK) goto fail;
        if (ready(s,1,start,5000000u)<=0) goto fail;
        error=0;
        rc=sceNetInetGetsockopt(s,SOL_SOCKET,SO_ERROR,&error,&option_size);
        record("tcp_connect completion_rc=%d socket_error=%d duration_ms=%u",rc,error,elapsed(start)/1000);
        if (rc < 0 || error) goto fail;
    }
    record("tcp_connect PASS scope=%s port=%d duration_ms=%u",scope,port,elapsed(start)/1000);
    return s;
fail:
    record("tcp_connect NOT_CONNECTED scope=%s port=%d duration_ms=%u",scope,port,elapsed(start)/1000);
    sceNetInetClose(s);
    return -1;
}

static void http_probe(const char *host, const char *path, struct in_addr ip, const char *source, int sony)
{
    int s, rc, error, sent=0, count=0, status=0, length=-1, te=0, eof=0;
    int size;
    unsigned char response[4096];
    char request[384];
    size_t body=0;
    unsigned int start;
    if (stopping()) return;
    record("http BEGIN host=%s path=%s ip=%s source=%s port=80",host,path,inet_ntoa(ip),source);
    s=tcp_connect(ip,80,source); if (s < 0) return;
    size=snprintf(request,sizeof(request),"GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: PSPNetconf-agent/1.0.0 libhttp/1.0.0\r\nAccept: */*;q=0.01\r\nConnection: close\r\n\r\n",path,host);
    if (size < 0 || (size_t)size >= sizeof(request)) { record("request overflow"); goto done; }
    start=sceKernelGetSystemTimeLow();
    while (sent < size && ready(s,1,start,3000000u)>0) {
        rc=sceNetInetSend(s,request+sent,size-sent,0);
        if (rc <= 0) {
            error=sceNetInetGetErrno();
            if (rc < 0 && (error==EAGAIN || error==EWOULDBLOCK)) continue;
            record("http_send FAIL rc=%d errno=%d",rc,error); goto done;
        }
        sent+=rc;
    }
    record("http_send bytes=%d requested=%d",sent,size);
    if (sent != size) goto done;
    start=sceKernelGetSystemTimeLow();
    while (count < (int)sizeof(response) && ready(s,0,start,5000000u)>0) {
        rc=sceNetInetRecv(s,response+count,sizeof(response)-(size_t)count,0);
        if (rc==0) { eof=1; break; }
        if (rc < 0) {
            error=sceNetInetGetErrno();
            if (error==EAGAIN || error==EWOULDBLOCK) continue;
            record("http_recv FAIL rc=%d errno=%d",rc,error); break;
        }
        count+=rc;
        status=nd_http_status(response,(size_t)count,&body,&length,&te);
        if (status < 0) break;
        if (status > 0 && (te || (length >= 0 && (size_t)count-body >= (size_t)length))) break;
    }
    record("http_response status=%d received_bytes=%d header_bytes=%u content_length=%d transfer_encoding=%d eof=%d duration_ms=%u",
           status,count,(unsigned int)body,length,te,eof,elapsed(start)/1000);
    if (sony) {
        int match=status==200 && !te && body>0 && length==1 && (size_t)count==body+1 && response[body]=='p';
        record("historical_sony_exact_200_p=%s (not a capture of XMB test)",match ? "PASS" : "NOT_CONFIRMED");
    }
    record("http: body/headers not logged; no redirects followed; HTTP response alone is not universal Internet validation");
done:
    sceNetInetClose(s);
}

typedef struct {
    int request, reading, size, rc;
    unsigned char data[2];
} HttpJob;
static HttpJob http_job;

static int http_thread(SceSize size, void *arg)
{
    (void)size; (void)arg;
    http_job.rc=http_job.reading ?
        sceHttpReadData(http_job.request,http_job.data,(unsigned int)http_job.size) :
        sceHttpSendRequest(http_job.request,NULL,0);
    return 0;
}

/* Only Send/Read perform network waits. Supervise them separately and retain
 * the SDK's original return before asking for the underlying network errno. */
static int http_operation(int request, int reading, int size, int *operation_rc)
{
    SceUID thread;
    SceUInt wait;
    unsigned int start=sceKernelGetSystemTimeLow();
    unsigned int limit=reading ? 6500000u : 25000000u;
    int rc;
    memset(&http_job,0,sizeof(http_job));
    http_job.request=request; http_job.reading=reading; http_job.size=size; http_job.rc=-1;
    record("native_http_%s BEGIN watchdog_ms=%u",reading ? "read" : "send",limit/1000);
    thread=sceKernelCreateThread("NETDIAG HTTP",http_thread,0x18,0x8000,PSP_THREAD_ATTR_USER,NULL);
    if (thread < 0) { result("http_thread_create",thread); return 0; }
    rc=sceKernelStartThread(thread,0,NULL);
    if (rc < 0) { result("http_thread_start",rc); sceKernelDeleteThread(thread); return 0; }
    for (;;) {
        wait=50000;
        rc=sceKernelWaitThreadEnd(thread,&wait);
        if (rc==0) break;
        drain_events();
        if (stopping() || elapsed(start)>=limit) {
            record("native_http_%s %s",reading ? "read" : "send",cancelled ? "CANCELLED" : "TIMEOUT");
            result("http_abort_request",sceHttpAbortRequest(request));
            wait=1000000;
            rc=sceKernelWaitThreadEnd(thread,&wait);
            if (rc < 0) {
                record("STOP HTTP worker did not stop; close report and exit without unloading live libraries");
                if (report >= 0) { sceIoClose(report); report=-1; }
                sceKernelExitGame(); sceKernelExitDeleteThread(0);
            }
            record("native_http return_after_abort=%d hex=0x%08x (cancellation output)",
                   http_job.rc,(unsigned int)http_job.rc);
            sceKernelDeleteThread(thread);
            return 0;
        }
    }
    *operation_rc=http_job.rc;
    record("native_http_%s rc=%d hex=0x%08x duration_ms=%u",reading ? "read" : "send",
           http_job.rc,(unsigned int)http_job.rc,elapsed(start)/1000);
    if (http_job.rc < 0) {
        int error=0;
        rc=sceHttpGetNetworkErrno(request,&error);
        record("native_http network_errno_api_rc=%d hex=0x%08x network_errno=%d network_hex=0x%08x",
               rc,(unsigned int)rc,error,(unsigned int)error);
    }
    sceKernelDeleteThread(thread);
    return 1;
}

static void native_http_version(int version)
{
    const char *url="http://fj00.psp.update.playstation.org/networktest/trial.txt";
    int template=-1, connection=-1, request=-1, rc, op_rc, status=0, length_rc;
    int total=0, eof=0;
    unsigned char body[2]={0};
    SceULong64 length=0;
    record("native_http BEGIN HTTP_version=%s historical_url=%s",version==PSP_HTTP_VERSION_1_0 ? "1.0" : "1.1",url);
    record("native_http scope=homebrew API comparison; XMB destination/options still unverified");
    template=sceHttpCreateTemplate("PSPNetconf-agent/1.0.0 libhttp/1.0.0",version,0);
    result("http_create_template",template); if (template < 0) goto done;
    /* Stop if controls cannot be applied: never continue with unknown waits,
     * redirects, cookies, authentication or proxy settings. */
#define HTTP_SETTING(label, call) do { rc=(call); result((label),rc); if (rc < 0) goto done; } while (0)
    HTTP_SETTING("http_resolve_timeout",sceHttpSetResolveTimeOut(template,3000000));
    HTTP_SETTING("http_resolve_retry",sceHttpSetResolveRetry(template,1));
    HTTP_SETTING("http_connect_timeout",sceHttpSetConnectTimeOut(template,5000000));
    HTTP_SETTING("http_send_timeout",sceHttpSetSendTimeOut(template,3000000));
    HTTP_SETTING("http_recv_timeout",sceHttpSetRecvTimeOut(template,5000000));
    HTTP_SETTING("http_disable_redirect",sceHttpDisableRedirect(template));
    HTTP_SETTING("http_disable_cookie",sceHttpDisableCookie(template));
    HTTP_SETTING("http_disable_auth",sceHttpDisableAuth(template));
    HTTP_SETTING("http_disable_keepalive",sceHttpDisableKeepAlive(template));
    HTTP_SETTING("http_direct_no_proxy",sceHttpSetProxy(template,0,PSP_HTTP_PROXY_AUTO,NULL,0));
    HTTP_SETTING("http_accept",sceHttpAddExtraHeader(template,"Accept","*/*;q=0.01",PSP_HTTP_HEADER_OVERWRITE));
    HTTP_SETTING("http_identity_encoding",sceHttpAddExtraHeader(template,"Accept-Encoding","identity",PSP_HTTP_HEADER_OVERWRITE));
#undef HTTP_SETTING
    connection=sceHttpCreateConnectionWithURL(template,url,0);
    result("http_create_connection",connection); if (connection < 0 || stopping()) goto done;
    request=sceHttpCreateRequestWithURL(connection,PSP_HTTP_METHOD_GET,(char *)url,0);
    result("http_create_request",request); if (request < 0 || stopping()) goto done;
    if (!http_operation(request,0,0,&op_rc) || op_rc < 0) goto done;
    rc=sceHttpGetStatusCode(request,&status); result("http_get_status",rc);
    if (rc < 0) goto done;
    length_rc=sceHttpGetContentLength(request,&length); result("http_get_length",length_rc);
    record("native_http status=%d content_length_known=%d content_length=%llu",status,length_rc>=0,(unsigned long long)length);
    while (total < 2 && !stopping()) {
        if (!http_operation(request,1,2-total,&op_rc) || op_rc < 0) goto done;
        if (op_rc==0) { eof=1; break; }
        if (op_rc > 2-total) { record("native_http invalid read size"); goto done; }
        memcpy(body+total,http_job.data,(size_t)op_rc); total+=op_rc;
    }
    record("native_http historical_sony_exact_200_p=%s observed_body_bytes=%d eof=%d (not an XMB capture)",
           status==200 && length_rc>=0 && length==1 && total==1 && body[0]=='p' && eof ? "PASS" : "NOT_CONFIRMED",total,eof);
done:
    if (request >= 0) result("http_delete_request",sceHttpDeleteRequest(request));
    if (connection >= 0) result("http_delete_connection",sceHttpDeleteConnection(connection));
    if (template >= 0) result("http_delete_template",sceHttpDeleteTemplate(template));
}

static void native_http_probes(void)
{
    int modules[]={PSP_NET_MODULE_PARSEURI,PSP_NET_MODULE_PARSEHTTP,PSP_NET_MODULE_HTTP};
    int loaded=0, initialized=0, rc, i;
    if (stopping()) return;
    for (i=0;i<3;++i) {
        rc=sceUtilityLoadNetModule(modules[i]);
        record("native_http load_module=%d rc=%d hex=0x%08x",modules[i],rc,(unsigned int)rc);
        if (rc < 0) goto done;
        ++loaded;
    }
    rc=sceHttpInit(128*1024); result("http_init",rc); if (rc < 0) goto done;
    initialized=1;
    native_http_version(PSP_HTTP_VERSION_1_0);
    if (!stopping()) native_http_version(PSP_HTTP_VERSION_1_1);
done:
    if (initialized) result("http_end",sceHttpEnd());
    while (loaded > 0) {
        --loaded; rc=sceUtilityUnloadNetModule(modules[loaded]);
        record("native_http unload_module=%d rc=%d hex=0x%08x",modules[loaded],rc,(unsigned int)rc);
    }
}

int main(int argc, char **argv)
{
    int rc, profile, common=0, inet_module=0, net=0, inet=0, apctl=0, handler=-1;
    int primary_ok, secondary_ok, public_ok, sony_ok, gateway_ok=0, quad9_ok;
    SceUID callbacks;
    char primary[16]={0}, secondary[16]={0}, gateway[16]={0};
    struct in_addr sony_ip, primary_ip, secondary_ip, public_ip, never_ip, fixed, gateway_ip, gateway_answer, quad9_ip;
    const char *sony="fj00.psp.update.playstation.org";
    (void)argc; (void)argv;
    pspDebugScreenInit(); epoch=sceKernelGetSystemTimeLow();
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    callbacks=sceKernelCreateThread("NETDIAG callbacks",callback_thread,0x11,0x1000,PSP_THREAD_ATTR_USER,NULL);
    if (callbacks >= 0) sceKernelStartThread(callbacks,0,NULL);
    if (!open_report()) {
        pspDebugScreenPrintf("Cannot create report in PSP/GAME/NETDIAG.\nPlace the app in that folder. Returning in 8s.\n");
        sceKernelDelayThread(8000000); sceKernelExitGame(); return 1;
    }
    record("PRO-X NETDIAG format=3 tool=1.0 build=%s",NETDIAG_BUILD_ID);
    record("firmware=0x%08x activation_of_CFW_not_verified_by_this_tool",sceKernelDevkitVersion());
    record("scope=standalone homebrew; no flash/profile edits; no Sony test patch");
    record("privacy=private local IPv4/gateway/DNS; no SSID/BSSID/MAC/passwords; review before sharing");
    record("wlan_switch=%d",sceWlanGetSwitchState());
    profile=choose_profile(); if (profile < 0) goto cleanup;
    {
        netData data;
        if (sceUtilityGetNetParam(profile,PSP_NETPARAM_IS_STATIC_IP,&data)==0)
            record("profile_static_ip=%u",data.asUint);
        if (sceUtilityGetNetParam(profile,PSP_NETPARAM_MANUAL_DNS,&data)==0)
            record("profile_manual_dns=%u",data.asUint);
    }
    rc=sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON); result("load_net_common",rc);
    if (rc < 0) goto cleanup;
    common=1;
    rc=sceUtilityLoadNetModule(PSP_NET_MODULE_INET); result("load_net_inet",rc);
    if (rc < 0) goto cleanup;
    inet_module=1;
    rc=sceNetInit(256*1024,0x30,4096,0x30,4096); result("net_init",rc);
    if (rc < 0) goto cleanup;
    net=1;
    rc=sceNetInetInit(); result("inet_init",rc); if (rc < 0) goto cleanup; inet=1;
    rc=sceNetApctlInit(0x1800,0x30); result("apctl_init",rc); if (rc < 0) goto cleanup; apctl=1;
    rc=sceNetResolverInit(); result("resolver_init",rc); resolver_ready=rc>=0;
    event_lock=sceKernelCreateSema("NETDIAG events",0,1,1,NULL);
    if (event_lock >= 0) { handler=sceNetApctlAddHandler(ap_event,NULL); result("apctl_handler",handler); }
    if (!connect_profile(profile)) goto cleanup;
    info_address(PSP_NET_APCTL_INFO_IP,"local_ip",NULL);
    info_address(PSP_NET_APCTL_INFO_SUBNETMASK,"netmask",NULL);
    info_address(PSP_NET_APCTL_INFO_GATEWAY,"gateway",gateway);
    info_address(PSP_NET_APCTL_INFO_PRIMDNS,"primary_dns",primary);
    info_address(PSP_NET_APCTL_INFO_SECDNS,"secondary_dns",secondary);
    {
        union SceNetApctlInfo info;
        memset(&info,0,sizeof(info));
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_CHANNEL,&info)==0) record("channel=%u",info.channel);
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_STRENGTH,&info)==0) record("signal_percent=%u",info.strength);
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_SECURITY_TYPE,&info)==0)
            record("security_api_code=%u (not proof of WPA2 negotiation)",info.securityType);
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_USE_PROXY,&info)==0)
            record("profile_proxy_enabled=%u (probes connect directly, proxy is not used)",info.useProxy);
    }
    if (stopping()) goto cleanup;
    /* A local response helps distinguish LAN communication from WAN/DNS.
     * Closed/filtered admin ports or no DNS proxy can make these inconclusive. */
    if (gateway[0] && strcmp(gateway,"0.0.0.0") && inet_aton(gateway,&gateway_ip)) {
        int socket;
        http_probe(gateway,"/",gateway_ip,"local gateway; no DNS",0);
        socket=tcp_connect(gateway_ip,443,"local gateway; TCP only, no TLS");
        if (socket >= 0) sceNetInetClose(socket);
        gateway_ok=direct_dns(gateway,sony,&gateway_answer);
    } else record("gateway probes SKIP: no valid gateway IPv4");
    if (stopping()) goto cleanup;
    /* Literal-IP control is intentionally independent of DNS. Any HTTP status
     * is logged; a redirect is a response, not a successful Sony test. */
    inet_aton("1.1.1.1",&fixed);
    http_probe("1.1.1.1","/",fixed,"literal IPv4; no DNS",0);
    {
        int socket;
        inet_aton("9.9.9.9",&fixed);
        socket=tcp_connect(fixed,53,"Quad9 DNS TCP; literal IPv4, no DNS lookup");
        if (socket >= 0) sceNetInetClose(socket);
    }
    if (stopping()) goto cleanup;
    sony_ok=native_dns(sony,&sony_ip);
    primary_ok=direct_dns(primary,sony,&primary_ip);
    if (!strcmp(secondary,primary)) { secondary_ok=0; record("secondary direct DNS SKIP duplicate primary"); }
    else secondary_ok=direct_dns(secondary,sony,&secondary_ip);
    if (!strcmp(primary,"1.1.1.1")) {
        public_ok=primary_ok;
        if (public_ok) public_ip=primary_ip;
        record("Cloudflare DNS reuse primary probe result");
    } else if (!strcmp(secondary,"1.1.1.1")) {
        public_ok=secondary_ok;
        if (public_ok) public_ip=secondary_ip;
        record("Cloudflare DNS reuse secondary probe result");
    }
    else public_ok=direct_dns("1.1.1.1",sony,&public_ip);
    quad9_ok=direct_dns("9.9.9.9",sony,&quad9_ip);
    if (sony_ok) http_probe(sony,"/networktest/trial.txt",sony_ip,"native resolver",1);
    else if (primary_ok) http_probe(sony,"/networktest/trial.txt",primary_ip,"direct configured primary DNS",1);
    else if (secondary_ok) http_probe(sony,"/networktest/trial.txt",secondary_ip,"direct configured secondary DNS",1);
    else if (gateway_ok) http_probe(sony,"/networktest/trial.txt",gateway_answer,"direct gateway DNS (diagnostic only)",1);
    else if (public_ok) http_probe(sony,"/networktest/trial.txt",public_ip,"direct public DNS (diagnostic only)",1);
    else if (quad9_ok) http_probe(sony,"/networktest/trial.txt",quad9_ip,"direct Quad9 DNS (diagnostic only)",1);
    else record("Sony HTTP SKIP: no resolved IPv4; native DNS failure is preserved");
    /* Preserve the previous gateway/raw probe sequence; compare the native HTTP
     * API immediately afterwards using its own hostname resolution. */
    native_http_probes();
    if (!stopping() && native_dns("neverssl.com",&never_ip))
        http_probe("neverssl.com","/online.html",never_ip,"native resolver",0);
    else record("NeverSSL HTTP SKIP: no resolved IPv4 or cancelled");
cleanup:
    drain_events();
    record("tests_finished cancelled=%d log_failed=%d event_drops=%d",cancelled,log_failed,events_dropped);
    if (handler >= 0) result("remove_apctl_handler",sceNetApctlDelHandler(handler));
    if (apctl) result("apctl_disconnect",sceNetApctlDisconnect());
    if (resolver_ready) result("resolver_term",sceNetResolverTerm());
    if (apctl) result("apctl_term",sceNetApctlTerm());
    if (inet) result("inet_term",sceNetInetTerm());
    if (net) result("net_term",sceNetTerm());
    if (inet_module) result("unload_net_inet",sceUtilityUnloadNetModule(PSP_NET_MODULE_INET));
    if (common) result("unload_net_common",sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON));
    record("END hardware_success_not_inferred; send this private report for analysis");
    if (report >= 0) {
        rc=sceIoClose(report); report=-1;
        if (rc < 0) log_failed=1;
    }
    pspDebugScreenPrintf("\n%s\n%s\n",log_failed ? "Report write/close FAILED" : "Report closed:",report_path);
    pspDebugScreenPrintf("X: return to XMB (automatic in 20 seconds).\n");
    {
        unsigned int start=sceKernelGetSystemTimeLow();
        while (!home_requested && elapsed(start)<20000000u) {
            SceCtrlData pad;
            memset(&pad,0,sizeof(pad)); sceCtrlPeekBufferPositive(&pad,1);
            if (pad.Buttons & PSP_CTRL_CROSS) break;
            sceKernelDelayThread(20000);
        }
    }
    sceKernelExitGame();
    return 0;
}
