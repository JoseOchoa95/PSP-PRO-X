/* Kernel-owned messages, no retained user pointers/callbacks. GPL-3.0-or-later. */
#include "native_query_state.h"
#include <string.h>
void prox_nq_reset(ProxNativeQueryState *s,unsigned int lease)
{ memset(s,0,sizeof(*s)); s->lease=lease?lease:1; }
int prox_nq_open(ProxNativeQueryState *s)
{
    if(s->fault || s->active) return -1;
    if(++s->generation==0) ++s->generation;
    s->done=0; s->cancel=0; s->command=PROX_NQ_IDLE; s->phase=PROX_NQ_QUERY;
    s->downloaded=0; s->consumer=0; memset(&s->answer,0,sizeof(s->answer)); return 0;
}
int prox_nq_submit(ProxNativeQueryState *s)
{
    if(!s->ready || s->fault || s->active || s->cancel || !s->generation) return -1;
    s->active=1; s->done=0; s->command=PROX_NQ_CHECK; return 0;
}
int prox_nq_download(ProxNativeQueryState *s,int consumer)
{
    if(consumer<=0 || s->fault || s->cancel || s->active || !s->ready || !s->done ||
       s->phase!=PROX_NQ_QUERY || s->answer.result!=1 ||
       s->answer.package_size<=32768 || s->answer.package_size>16u*1024u*1024u) return -1;
    s->consumer=consumer; s->active=1; s->done=0; s->downloaded=0;
    s->phase=PROX_NQ_TRANSFER; s->command=PROX_NQ_DOWNLOAD; return 0;
}
int prox_nq_handoff(ProxNativeQueryState *s,int consumer)
{
    if(consumer<=0 || consumer!=s->consumer || s->fault || s->cancel || !s->active ||
       s->phase!=PROX_NQ_VERIFIED || s->downloaded!=s->answer.package_size) return -1;
    s->phase=PROX_NQ_HANDOFF; s->command=PROX_NQ_INSTALL; return 0;
}
void prox_nq_cancel(ProxNativeQueryState *s) { s->cancel=1; }
int prox_nq_exchange(ProxNativeQueryState *s,int thread,int action,ProxNativeQueryMessage *m)
{
    if(!m || m->protocol!=PROX_NQ_PROTOCOL || m->lease!=s->lease || thread<=0) return -1;
    if(action==PROX_NQ_REGISTER) {
        if(s->fault || (s->owner && s->owner!=thread)) return -1;
        s->owner=thread; s->ready=1; return 0;
    }
    if(s->owner!=thread || !s->ready) return -1;
    if(action==PROX_NQ_POLL || action==PROX_NQ_STATUS) {
        m->generation=s->generation; m->cancel=(unsigned int)(s->cancel||s->fault);
        m->command=s->active?s->command:PROX_NQ_IDLE;
        m->phase=s->phase; m->downloaded=s->downloaded; return 0;
    }
    if(action==PROX_NQ_FAULT) {
        s->fault=1; s->cancel=1; s->active=0; s->done=1; s->answer.result=-1; return 0;
    }
    if(!s->active || m->generation!=s->generation) return -1;
    if(action==PROX_NQ_PROGRESS) {
        if(s->phase!=PROX_NQ_TRANSFER || s->cancel || s->fault ||
           m->downloaded<s->downloaded || m->downloaded>s->answer.package_size) return -1;
        s->downloaded=m->downloaded; return 0;
    }
    if(action==PROX_NQ_STAGED) {
        if(s->phase!=PROX_NQ_TRANSFER || s->cancel || s->fault ||
           s->downloaded!=s->answer.package_size) return -1;
        s->phase=PROX_NQ_VERIFIED; s->command=PROX_NQ_IDLE; return 0;
    }
    if(action!=PROX_NQ_RESULT || m->result < -1 || m->result>1 ||
       m->failure>PROX_NQ_CAUSE_CLIENT || (m->result>=0 && m->failure) ||
       !memchr(m->version,0,sizeof(m->version)) || !memchr(m->summary,0,sizeof(m->summary))) return -1;
    if(s->phase!=PROX_NQ_QUERY) {
        if(m->result!=-1) return -1; /* Download/launch cannot report installation success. */
        s->phase=PROX_NQ_FAILED; s->answer.result=-1; s->answer.failure=m->failure; s->active=0; s->done=1;
        s->command=PROX_NQ_IDLE; return 0;
    }
    s->answer=*m;
    if(s->cancel) s->answer.result=-1;
    s->active=0; s->done=1; s->command=PROX_NQ_IDLE; return 0;
}
