/* Retry only explicitly classified DNS/TCP failures, before TLS or HTTP.
 * The caller retains the original request deadline. GPL-3.0-or-later. */
#include "connect_retry.h"
#include "transport.h"
int prox_connect_retry(ProxRetryBudget *budget,const ProxConnectOps *ops,void *context)
{
    unsigned int attempt=1,i;
    int rc;
    for (;;) {
        if (ops->stopped(context)) return -1;
        prox_log("Connection attempt %u/5; session retries used=%u/4",attempt,budget->used);
        rc=ops->connect(context);
        if (!rc) return ops->stopped(context) ? -1 : 0;
        if (ops->close(context)) {
            prox_log("Connection cleanup failed; retry blocked"); return -1;
        }
        if (rc != PROX_CONNECT_RETRYABLE || ops->stopped(context)) return -1;
        if (budget->used >= 4) {
            prox_log("Connection retries exhausted for this updater run"); return -1;
        }
        prox_log("DNS/TCP attempt failed; retrying in 1 second (O cancels)");
        for (i=0;i<50;++i) {
            if (ops->stopped(context)) return -1;
            ops->pause(20000);
        }
        if (ops->stopped(context)) return -1;
        ++budget->used; ++attempt;
        if (ops->restart(context)) {
            prox_log("Fresh Wi-Fi session failed; retry stopped"); return -1;
        }
    }
}
