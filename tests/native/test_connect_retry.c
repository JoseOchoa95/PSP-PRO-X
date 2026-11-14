/* Production retry policy, virtual time and connection ownership. */
#include "connect_retry.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
static int calls,closes,failures,hard,close_error,stop_at,open_socket,restarts,restart_error;
static unsigned int ticks,deadline;
void prox_log(const char *f,...) { (void)f; }
static int connect_once(void *p)
{
    (void)p; assert(!open_socket); ++calls; open_socket=1;
    return hard ? -1 : calls <= failures ? PROX_CONNECT_RETRYABLE : 0;
}
static int close_once(void *p)
{ (void)p; ++closes; if (close_error) return -1; open_socket=0; return 0; }
static int stopped(void *p)
{ (void)p; return (stop_at && ticks >= (unsigned int)stop_at) || ticks >= deadline; }
static void pause_once(unsigned int us) { assert(us == 20000); ticks+=us; }
static int restart_once(void *p)
{ (void)p; assert(!open_socket); ++restarts; return restart_error ? -1 : 0; }
static const ProxConnectOps ops={connect_once,close_once,stopped,pause_once,restart_once};
static void reset(int n)
{ calls=closes=hard=close_error=stop_at=open_socket=restarts=restart_error=0; failures=n; ticks=0; deadline=90000000; }
int main(void)
{
    ProxRetryBudget b={0}; int n;
    for (n=0;n<5;++n) {
        reset(n); b.used=0; assert(!prox_connect_retry(&b,&ops,NULL));
        assert(calls == n+1 && closes == n && b.used == (unsigned int)n && ticks == (unsigned int)n*1000000);
        assert(open_socket && restarts == n); close_once(NULL);
    }
    reset(99); b.used=0; assert(prox_connect_retry(&b,&ops,NULL)<0);
    assert(calls == 5 && closes == 5 && b.used == 4 && ticks == 4000000 && !open_socket);
    /* Budget remains exhausted across requests/pages/redirects. */
    reset(99); assert(prox_connect_retry(&b,&ops,NULL)<0 && calls == 1 && !ticks);
    reset(0); assert(!prox_connect_retry(&b,&ops,NULL) && calls == 1); close_once(NULL);
    reset(99); b.used=0; stop_at=40000;
    assert(prox_connect_retry(&b,&ops,NULL)<0 && calls == 1 && ticks == 40000 && !b.used && !open_socket);
    reset(99); b.used=0; deadline=60000;
    assert(prox_connect_retry(&b,&ops,NULL)<0 && calls == 1 && ticks == 60000 && !open_socket);
    reset(99); b.used=0; deadline=0;
    assert(prox_connect_retry(&b,&ops,NULL)<0 && !calls && !closes);
    reset(99); b.used=0; hard=1;
    assert(prox_connect_retry(&b,&ops,NULL)<0 && calls == 1 && closes == 1 && !ticks);
    reset(99); b.used=0; restart_error=1;
    assert(prox_connect_retry(&b,&ops,NULL)<0 && calls == 1 && restarts == 1 && b.used == 1);
    reset(99); b.used=0; close_error=1;
    assert(prox_connect_retry(&b,&ops,NULL)<0 && calls == 1 && closes == 1 && !ticks && open_socket);
    puts("PASS: bounded five-attempt pre-TLS retry, shared invocation budget, success stop, cleanup, cancellation and deadline");
    return 0;
}
