#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "native_query_state.h"
int main(void)
{
    ProxNativeQueryState s; ProxNativeQueryMessage m={0}; unsigned int old;
    prox_nq_reset(&s,77); m.protocol=PROX_NQ_PROTOCOL; m.lease=77;
    assert(!prox_nq_open(&s) && prox_nq_submit(&s)<0);
    assert(!prox_nq_exchange(&s,2,PROX_NQ_REGISTER,&m));
    assert(prox_nq_exchange(&s,3,PROX_NQ_REGISTER,&m)<0);
    assert(!prox_nq_submit(&s));
    assert(prox_nq_open(&s)<0 && prox_nq_submit(&s)<0);
    assert(!prox_nq_exchange(&s,2,PROX_NQ_POLL,&m) && m.command==PROX_NQ_CHECK);
    m.result=0;
    assert(prox_nq_exchange(&s,3,PROX_NQ_RESULT,&m)<0);
    --m.generation; assert(prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m)<0); ++m.generation;
    prox_nq_cancel(&s);
    assert(!prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m) && s.answer.result==-1 && s.done && !s.active);
    assert(!prox_nq_open(&s) && !prox_nq_submit(&s));
    assert(!prox_nq_exchange(&s,2,PROX_NQ_POLL,&m)); old=m.generation;
    memset(m.version,'x',sizeof(m.version));
    assert(prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m)<0 && s.active);
    m.version[0]=0; m.result=2;
    assert(prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m)<0 && s.active);
    m.result=1; assert(!prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m) && s.answer.result==1);
    m.result=0; assert(s.answer.result==1); /* Packet lifetime is caller-owned. */
    assert(!prox_nq_open(&s) && s.generation!=old && !prox_nq_submit(&s));
    m.generation=old; assert(prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m)<0);
    assert(!prox_nq_exchange(&s,2,PROX_NQ_FAULT,&m) && s.fault && s.done && s.answer.result==-1);
    assert(prox_nq_open(&s)<0);
    prox_nq_reset(&s,77); m.lease=77; m.protocol=PROX_NQ_PROTOCOL;
    assert(!prox_nq_open(&s) && !prox_nq_exchange(&s,2,PROX_NQ_REGISTER,&m));
    assert(prox_nq_download(&s,1)<0 && !prox_nq_submit(&s));
    assert(!prox_nq_exchange(&s,2,PROX_NQ_POLL,&m));
    m.result=1; m.package_size=131072; strcpy(m.version,"0.3.1");
    assert(!prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m));
    assert(!prox_nq_download(&s,1) && s.phase==PROX_NQ_TRANSFER);
    assert(prox_nq_open(&s)<0 && prox_nq_download(&s,1)<0 && prox_nq_handoff(&s,1)<0);
    assert(!prox_nq_exchange(&s,2,PROX_NQ_POLL,&m) && m.command==PROX_NQ_DOWNLOAD);
    m.result=-1; m.failure=999;
    assert(prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m)<0 && s.active);
    m.failure=PROX_NQ_CAUSE_NONE;
    m.downloaded=32768; assert(prox_nq_exchange(&s,3,PROX_NQ_PROGRESS,&m)<0);
    assert(!prox_nq_exchange(&s,2,PROX_NQ_PROGRESS,&m));
    m.downloaded=1; assert(prox_nq_exchange(&s,2,PROX_NQ_PROGRESS,&m)<0);
    m.downloaded=131073; assert(prox_nq_exchange(&s,2,PROX_NQ_PROGRESS,&m)<0);
    assert(prox_nq_exchange(&s,2,PROX_NQ_STAGED,&m)<0);
    m.downloaded=131072; assert(!prox_nq_exchange(&s,2,PROX_NQ_PROGRESS,&m));
    assert(!prox_nq_exchange(&s,2,PROX_NQ_STAGED,&m) && s.active && s.phase==PROX_NQ_VERIFIED);
    assert(prox_nq_handoff(&s,3)<0 && !prox_nq_handoff(&s,1));
    assert(prox_nq_handoff(&s,1)<0 && prox_nq_open(&s)<0);
    m.result=1; assert(prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m)<0); /* No install-success packet. */
    m.result=-1; assert(!prox_nq_exchange(&s,2,PROX_NQ_RESULT,&m));
    assert(!s.active && s.phase==PROX_NQ_FAILED && !prox_nq_open(&s));
    prox_nq_reset(&s,88); m.lease=77;
    assert(prox_nq_exchange(&s,2,PROX_NQ_REGISTER,&m)<0);
    puts("PASS: native query leases/worker ownership/generations, cancellation cannot become current, fault/invalid results fail closed");
    return 0;
}
