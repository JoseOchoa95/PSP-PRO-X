/* Bounded pre-TLS retries; GPL-3.0-or-later. */
#ifndef PROX_CONNECT_RETRY_H
#define PROX_CONNECT_RETRY_H
#define PROX_CONNECT_RETRYABLE (-2)
typedef struct { unsigned int used; } ProxRetryBudget;
typedef struct {
    int (*connect)(void *);
    int (*close)(void *);
    int (*stopped)(void *);
    void (*pause)(unsigned int);
    int (*restart)(void *);
} ProxConnectOps;
/* Four extra attempts shared across the complete updater invocation. */
int prox_connect_retry(ProxRetryBudget *,const ProxConnectOps *,void *);
#endif
