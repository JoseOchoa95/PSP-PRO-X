/* PRO-X bounded HTTPS transport; GPL-3.0-or-later. */
#ifndef PROX_TRANSPORT_H
#define PROX_TRANSPORT_H
#include <stddef.h>
typedef struct {
    int status;
    size_t bytes;
    char location[2048];
    char remaining[24], reset[24], retry_after[24];
    unsigned char sha256[32];
} ProxResponse;
typedef int (*ProxSink)(const unsigned char *, size_t, void *);
/* 0 success, negative error/cancel; sink only receives 200 response bodies. */
int prox_https(const char *url, const char *ca_file, size_t limit,
               ProxSink sink, void *sink_arg, ProxResponse *response);
void prox_log(const char *format, ...);
int prox_stopping(void);
#endif
