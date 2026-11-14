/* PRO-X user-mode authenticated HTTPS probe, GPL-3.0-or-later.
 * No automatic redirects: every destination is checked before DNS/connect.
 * Mbed TLS checks the chain, validity dates and hostname, with SNI enabled. */
#ifdef PROX_HTTPS_NATIVE
#include "update_transport_shim.h"
#else
#include <pspkernel.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#endif
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_time.h"
#include "bridge.h"
#include "transport.h"
#include "resolver.h"
#include "connect_retry.h"
#include "network_session.h"
#include "policy.h"
#include "prox_product.h"

#ifndef PROX_HTTPS_NATIVE
_Static_assert(sizeof(struct timeval) == 8, "PSP select timeval ABI mismatch");
#endif

typedef struct {
    int socket;
    unsigned int start;
    mbedtls_ssl_context ssl;
} Connection;
/* Process lifetime is one updater invocation; all requests/redirects share this
 * budget so a failing catalog cannot multiply five attempts per page/asset. */
static ProxRetryBudget retry_budget;
typedef struct { Connection *connection; const char *host; } ConnectContext;

mbedtls_ms_time_t mbedtls_ms_time(void)
{ return (mbedtls_ms_time_t)(sceKernelGetSystemTimeWide()/1000); }

static size_t live_memory, peak_memory;
static void *tls_calloc(size_t count, size_t size)
{
    size_t bytes;
    size_t *block;
    if (size && count > (2*1024*1024u-16)/size) return NULL;
    bytes = count*size;
    if (bytes > 2*1024*1024u-16 || live_memory > 2*1024*1024u-bytes-16) return NULL;
    block = calloc(1, bytes+16);
    if (!block) return NULL;
    *block = bytes+16; live_memory += *block;
    if (live_memory > peak_memory) peak_memory = live_memory;
    return (unsigned char *)block+16;
}
static void tls_free(void *pointer)
{
    if (pointer) {
        size_t *block = (void *)((unsigned char *)pointer-16);
        live_memory -= *block;
        free(block);
    }
}

static int expired(Connection *c)
{ return prox_stopping() || (unsigned int)(sceKernelGetSystemTimeLow() - c->start) > 90000000u; }

static int entropy(void *context, unsigned char *bytes, size_t size)
{
    (void)context;
    if (size > 256) return -1;
    return proxUpdateRandom(bytes, (unsigned int)size);
}

static int send_tls(void *context, const unsigned char *bytes, size_t size)
{
    Connection *c = context;
    int rc = sceNetInetSend(c->socket, bytes, size, 0);
    if (rc >= 0) return rc;
    rc = sceNetInetGetErrno();
    return rc == EAGAIN || rc == EWOULDBLOCK ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_NET_SEND_FAILED;
}

static int recv_tls(void *context, unsigned char *bytes, size_t size)
{
    Connection *c = context;
    int rc = sceNetInetRecv(c->socket, bytes, size, 0);
    if (rc >= 0) return rc;
    rc = sceNetInetGetErrno();
    return rc == EAGAIN || rc == EWOULDBLOCK ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_RECV_FAILED;
}

static int again(int rc)
{ return rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE; }

static int read_tls(Connection *c, unsigned char *bytes, size_t size)
{
    int rc;
    while (!expired(c)) {
        rc = mbedtls_ssl_read(&c->ssl, bytes, size);
        if (!again(rc)) return rc;
        sceKernelDelayThread(10000);
    }
    return -1;
}

static int line(Connection *c, char *buffer, size_t capacity)
{
    size_t n = 0;
    unsigned char ch;
    while (n + 1 < capacity) {
        if (read_tls(c, &ch, 1) != 1) return -1;
        buffer[n++] = (char)ch;
        if (ch == '\n') {
            if (n < 2 || buffer[n-2] != '\r') return -1;
            buffer[n-2] = 0;
            return (int)n;
        }
        if (!ch || ch == 127 || (ch < 32 && ch != '\r' && ch != '\t')) return -1;
    }
    return -1;
}

static int resolve(Connection *c, const char *host, struct in_addr *address)
{
#ifdef PROX_HTTPS_NATIVE
    int resolver, rc;
    char scratch[1024];
    if (sceNetResolverCreate(&resolver, scratch, sizeof(scratch)) < 0) return -1;
    rc = sceNetResolverStartNtoA(resolver, host, address, 5, 1);
    if (sceNetResolverDelete(resolver) < 0 || expired(c)) return -1;
    return rc < 0 ? PROX_CONNECT_RETRYABLE : rc ? -1 : 0;
#else
    return prox_psp_resolve(host,address,c->start);
#endif
}

static int tcp(Connection *c, const char *host)
{
    struct sockaddr_in address;
    int rc, nonblock = 1;
    unsigned int start;
    memset(&address, 0, sizeof(address));
    rc=resolve(c, host, &address.sin_addr);
    if (rc) return rc;
#ifndef PROX_HTTPS_NATIVE
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET; address.sin_port = htons(443);
    c->socket = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    if (c->socket < 0 || sceNetInetSetsockopt(c->socket, SOL_SOCKET, SO_NONBLOCK,
        &nonblock, sizeof(nonblock)) < 0) return -1;
    rc = sceNetInetConnect(c->socket, (struct sockaddr *)&address, sizeof(address));
    if (rc == 0) return 0;
    rc = sceNetInetGetErrno();
    if (rc != EINPROGRESS && rc != EALREADY && rc != EAGAIN) return PROX_CONNECT_RETRYABLE;
    start = sceKernelGetSystemTimeLow();
    while (!expired(c) && (unsigned int)(sceKernelGetSystemTimeLow()-start) < 15000000u) {
        fd_set writes, errors;
        struct timeval wait = {0, 100000};
        int error = 0; socklen_t length = sizeof(error);
        if (c->socket >= FD_SETSIZE) return -1;
        FD_ZERO(&writes); FD_ZERO(&errors);
        FD_SET(c->socket, &writes); FD_SET(c->socket, &errors);
        rc = sceNetInetSelect(c->socket+1, NULL, &writes, &errors, &wait);
        if (rc < 0) return PROX_CONNECT_RETRYABLE;
        if (rc > 0) {
            if (sceNetInetGetsockopt(c->socket, SOL_SOCKET, SO_ERROR, &error, &length) < 0) return -1;
            if (error) return PROX_CONNECT_RETRYABLE;
            return 0;
        }
    }
    return expired(c) ? -1 : PROX_CONNECT_RETRYABLE;
}

static int connect_once(void *arg)
{ ConnectContext *ctx=arg; return tcp(ctx->connection,ctx->host); }
static int connect_close(void *arg)
{
    ConnectContext *ctx=arg; Connection *c=ctx->connection;
    if (c->socket < 0) return 0;
    if (sceNetInetClose(c->socket) < 0) return -1;
    c->socket=-1; return 0;
}
static int connect_stopped(void *arg)
{ ConnectContext *ctx=arg; return expired(ctx->connection); }
static void connect_pause(unsigned int us) { sceKernelDelayThread(us); }
static int connect_restart(void *arg)
{
#ifdef PROX_HTTPS_NATIVE
    (void)arg; return 0; /* Real session lifecycle has its own production-code suite. */
#else
    ConnectContext *ctx=arg; return prox_network_retry(ctx->connection->start);
#endif
}
static int connect_with_retries(Connection *c,const char *host)
{
    ConnectContext ctx={c,host};
    static const ProxConnectOps ops={connect_once,connect_close,connect_stopped,connect_pause,connect_restart};
    return prox_connect_retry(&retry_budget,&ops,&ctx);
}

static int value(char *destination, size_t capacity, const char *source)
{
    while (*source == ' ' || *source == '\t') ++source;
    if (strlen(source) >= capacity) return -1;
    strcpy(destination, source); return 0;
}

static int number(const char *s, size_t *out, int base)
{
    size_t n = 0;
    int any = 0;
    while (*s) {
        unsigned int digit;
        if (*s >= '0' && *s <= '9') digit = (unsigned int)(*s-'0');
        else if (*s >= 'a' && *s <= 'f') digit = (unsigned int)(*s-'a'+10);
        else if (*s >= 'A' && *s <= 'F') digit = (unsigned int)(*s-'A'+10);
        else return -1;
        if (digit >= (unsigned int)base || n > (0x7fffffffu - digit) / (unsigned int)base) return -1;
        n = n * (unsigned int)base + digit; any = 1; ++s;
    }
    *out = n; return any ? 0 : -1;
}

static int headers(Connection *c, ProxResponse *r, size_t *length, int *chunked)
{
    /* GitHub's legitimate CSP header exceeds 4 KiB (4175 bytes observed).
     * Keep a bounded 8 KiB line buffer, 16 KiB aggregate header budget and
     * the existing smaller limits on Location/rate-limit/framing fields. */
    char buffer[8192];
    size_t total = 0;
    int n, have_length = 0;
    *length = 0; *chunked = 0;
    n = line(c, buffer, sizeof(buffer));
    if (n < 0 || (strncmp(buffer, "HTTP/1.1 ", 9) && strncmp(buffer, "HTTP/1.0 ", 9)) ||
        strlen(buffer) < 12 || buffer[9] < '1' || buffer[9] > '5' ||
        buffer[10] < '0' || buffer[10] > '9' || buffer[11] < '0' || buffer[11] > '9') return -1;
    r->status = (buffer[9]-'0')*100 + (buffer[10]-'0')*10 + buffer[11]-'0';
    total = (size_t)n; /* status line belongs to the aggregate budget too */
    while ((n = line(c, buffer, sizeof(buffer))) > 0) {
        char *colon;
        total += (size_t)n;
        if (total > 16384) { prox_log("HTTP headers exceed 16384-byte total limit"); return -1; }
        if (n == 2) break;
        colon = strchr(buffer, ':');
        if (!colon) return -1;
        *colon++ = 0;
        while (*colon == ' ' || *colon == '\t') ++colon;
        if (!strcasecmp(buffer, "Content-Length")) {
            if (have_length++ || number(colon, length, 10)) return -1;
        } else if (!strcasecmp(buffer, "Transfer-Encoding")) {
            if (*chunked || strcasecmp(colon, "chunked")) return -1;
            *chunked = 1;
        } else if (!strcasecmp(buffer, "Content-Encoding")) {
            if (strcasecmp(colon, "identity")) return -1;
        } else if (!strcasecmp(buffer, "Location")) {
            if (r->location[0] || value(r->location, sizeof(r->location), colon)) return -1;
        } else if (!strcasecmp(buffer, "X-RateLimit-Remaining")) {
            if (value(r->remaining, sizeof(r->remaining), colon)) return -1;
        } else if (!strcasecmp(buffer, "X-RateLimit-Reset")) {
            if (value(r->reset, sizeof(r->reset), colon)) return -1;
        } else if (!strcasecmp(buffer, "Retry-After")) {
            if (value(r->retry_after, sizeof(r->retry_after), colon)) return -1;
        }
    }
    if (n < 0) {
        prox_log("HTTP header line incomplete, malformed, or exceeds 8191 bytes");
        return -1;
    }
    if (have_length && *chunked) return -1;
    /* Require explicit framing; never accept truncation as successful EOF. */
    return have_length || *chunked || r->status != 200 ? 0 : -1;
}

int prox_https(const char *url, const char *ca_file, size_t limit,
               ProxSink sink, void *sink_arg, ProxResponse *response)
{
    char host[80], current[2048], request[2560], error[160];
    const char *path;
    unsigned char buffer[4096], *pem = NULL;
    Connection c;
    mbedtls_ssl_config config;
    mbedtls_x509_crt ca;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_sha256_context sha;
    FILE *file = NULL;
    long pem_size;
    int rc = -1, hop;
    memset(response, 0, sizeof(*response));
    if (strlen(url) >= sizeof(current)) return -1;
    strcpy(current, url);
    live_memory = 0; peak_memory = 0;
    if (mbedtls_platform_set_calloc_free(tls_calloc, tls_free)) return -1;
    memset(&c, 0, sizeof(c)); c.socket = -1;
    c.start = sceKernelGetSystemTimeLow();
    mbedtls_ssl_init(&c.ssl); mbedtls_ssl_config_init(&config);
    mbedtls_x509_crt_init(&ca); mbedtls_ctr_drbg_init(&drbg); mbedtls_sha256_init(&sha);
    file = fopen(ca_file, "rb");
    if (!file || fseek(file, 0, SEEK_END) || (pem_size = ftell(file)) < 1 || pem_size > 512*1024 ||
        fseek(file, 0, SEEK_SET)) goto done;
    pem = malloc((size_t)pem_size + 1);
    if (!pem || fread(pem, 1, (size_t)pem_size, file) != (size_t)pem_size) goto done;
    fclose(file); file = NULL; pem[pem_size] = 0;
    rc = mbedtls_x509_crt_parse(&ca, pem, (size_t)pem_size+1);
    free(pem); pem = NULL;
    if (rc < 0) goto done;
    prox_log("CA parse skipped=%d; chain, dates, hostname REQUIRED", rc);
    if (rc != 0) {
        prox_log("CA bundle incomplete: unsupported or invalid certificate(s)");
        rc = -1; goto done;
    }
    rc = mbedtls_ctr_drbg_seed(&drbg, entropy, NULL, (const unsigned char *)"PRO-X HTTPS", 11);
    if (rc) goto done;
    rc = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT,
          MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc) goto done;
    mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&config, &ca, NULL);
    mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
    for (hop = 0; hop <= 4; ++hop) {
        size_t length, received = 0;
        int chunked;
        memset(response, 0, sizeof(*response));
        rc = -1;
        if (prox_url(current, host, sizeof(host), &path) || expired(&c)) goto done;
        prox_log("HTTPS host=%s hop=%d (signed query omitted)", host, hop);
        if (connect_with_retries(&c, host)) goto done;
        if ((rc = mbedtls_ssl_setup(&c.ssl, &config)) != 0 ||
            (rc = mbedtls_ssl_set_hostname(&c.ssl, host)) != 0) goto done;
        mbedtls_ssl_set_bio(&c.ssl, &c, send_tls, recv_tls, NULL);
        while (again(rc = mbedtls_ssl_handshake(&c.ssl)) && !expired(&c)) sceKernelDelayThread(10000);
        if (rc || expired(&c) || mbedtls_ssl_get_verify_result(&c.ssl)) {
            unsigned int flags = mbedtls_ssl_get_verify_result(&c.ssl);
            if (flags && flags != 0xffffffffu) {
                prox_log("TLS certificate flags=0x%08x", flags);
                mbedtls_x509_crt_verify_info(error, sizeof(error), "", flags);
                prox_log("TLS certificate detail: %s", error);
            }
            if (!rc) rc = -1;
            goto done;
        }
        prox_log("TLS PASS protocol=%s cipher=%s verify_flags=0", mbedtls_ssl_get_version(&c.ssl),
                 mbedtls_ssl_get_ciphersuite(&c.ssl));
        rc = snprintf(request, sizeof(request),
            "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: PRO-X/" PROX_PRODUCT_VERSION "-https-milestone\r\n"
            "Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n"
            "Accept-Encoding: identity\r\nConnection: close\r\n\r\n", path, host);
        if (rc < 0 || (size_t)rc >= sizeof(request)) { rc = -1; goto done; }
        length = (size_t)rc;
        while (received < length && !expired(&c)) {
            rc = mbedtls_ssl_write(&c.ssl, (unsigned char *)request+received, length-received);
            if (again(rc)) { sceKernelDelayThread(10000); continue; }
            if (rc <= 0) goto done;
            received += (size_t)rc;
        }
        if (expired(&c)) { rc = -1; goto done; }
        rc = -1;
        if (headers(&c, response, &length, &chunked)) {
            prox_log("HTTP response headers rejected (line cap 8191; total cap 16384)");
            goto done;
        }
        prox_log("HTTP status=%d remaining=%s reset=%s retry-after=%s", response->status,
                 response->remaining, response->reset, response->retry_after);
        if (response->status == 301 || response->status == 302 || response->status == 303 ||
            response->status == 307 || response->status == 308) {
            if (hop == 4 || !response->location[0] ||
                prox_url(response->location, host, sizeof(host), &path)) goto done;
            strcpy(current, response->location);
            sceNetInetClose(c.socket); c.socket = -1;
            mbedtls_ssl_free(&c.ssl); mbedtls_ssl_init(&c.ssl);
            continue;
        }
        if (response->status != 200) { rc = 0; goto done; }
        if (length > limit) goto done;
        mbedtls_sha256_starts(&sha, 0);
        for (;;) {
            size_t remaining = length;
            if (chunked) {
                char chunk[40];
                if (line(&c, chunk, sizeof(chunk)) < 0 || number(chunk, &remaining, 16)) goto done;
                if (!remaining) {
                    /* No unbounded trailers accepted by this protocol. */
                    if (line(&c, chunk, sizeof(chunk)) != 2) goto done;
                    break;
                }
            }
            if (remaining > limit - response->bytes) goto done;
            while (remaining) {
                int n = read_tls(&c, buffer, remaining < sizeof(buffer) ? remaining : sizeof(buffer));
                if (n <= 0 || (sink && sink(buffer, (size_t)n, sink_arg))) goto done;
                if (mbedtls_sha256_update(&sha, buffer, (size_t)n)) goto done;
                response->bytes += (size_t)n; remaining -= (size_t)n;
                if (expired(&c)) goto done;
                if (response->bytes/32768 != (response->bytes-(size_t)n)/32768)
                    prox_log("Download progress: %u bytes", (unsigned int)response->bytes);
            }
            if (!chunked) break;
            if (line(&c, request, 4) != 2) goto done;
        }
        if (mbedtls_sha256_finish(&sha, response->sha256)) goto done;
        prox_log("body bytes=%u complete", (unsigned int)response->bytes);
        rc = 0; goto done;
    }
done:
    if (rc) {
        mbedtls_strerror(rc, error, sizeof(error));
        prox_log("HTTPS FAIL rc=%d %s; cancelled=%d", rc, error, prox_stopping());
    }
    if (file) fclose(file);
    free(pem);
    if (c.socket >= 0) sceNetInetClose(c.socket);
    mbedtls_ssl_free(&c.ssl); mbedtls_ssl_config_free(&config); mbedtls_x509_crt_free(&ca);
    mbedtls_ctr_drbg_free(&drbg); mbedtls_sha256_free(&sha);
    prox_log("TLS memory peak_bytes=%u remaining_bytes=%u cap=2097152", (unsigned int)peak_memory,
             (unsigned int)live_memory);
    return rc;
}
