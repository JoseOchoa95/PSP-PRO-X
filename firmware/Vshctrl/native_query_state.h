#ifndef PROX_NATIVE_QUERY_STATE_H
#define PROX_NATIVE_QUERY_STATE_H
#include "prox_native_query.h"
typedef struct {
    unsigned int lease,generation; int owner,ready,active,done,cancel,fault;
    unsigned int command,phase,downloaded; int consumer;
    ProxNativeQueryMessage answer;
} ProxNativeQueryState;
void prox_nq_reset(ProxNativeQueryState *,unsigned int lease);
int prox_nq_open(ProxNativeQueryState *);
int prox_nq_submit(ProxNativeQueryState *);
#ifdef PROX_NATIVE_DEDICATED_OTA
int prox_nq_start_boot_cleanup(ProxNativeQueryState *);
#endif
int prox_nq_download(ProxNativeQueryState *,int consumer);
int prox_nq_handoff(ProxNativeQueryState *,int consumer);
void prox_nq_cancel(ProxNativeQueryState *);
int prox_nq_exchange(ProxNativeQueryState *,int thread,int action,ProxNativeQueryMessage *);
#endif
