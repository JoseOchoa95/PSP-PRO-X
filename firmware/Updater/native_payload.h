/* Data-only reader for authenticated PRO launcher/installer payloads. */
#ifndef PROX_NATIVE_PAYLOAD_H
#define PROX_NATIVE_PAYLOAD_H
#include <stddef.h>
#include "prox_native_install.h"
typedef int (*ProxNativePayloadSink)(unsigned int,const unsigned char *,unsigned int,void *);
typedef struct {
    unsigned int found,containers,candidates;size_t inflated;
    ProxNativePayloadSink sink;void *context;
} ProxNativePayloadScan;
/* Bounded ELF/~PSP identity and complete file extent, no code execution. */
int prox_native_image(const unsigned char *,size_t,char [28],unsigned int *);
/* Scan loaded user launcher segments for its two original gzip arrays. */
int prox_native_payload_scan(ProxNativePayloadScan *,const unsigned char *,size_t);
int prox_native_payload_done(const ProxNativePayloadScan *);
extern const char *const prox_native_payload_names[PROX_NI_PARTS];
#endif
