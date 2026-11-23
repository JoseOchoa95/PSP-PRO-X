/* Role/generation-owned production write boundary; serialized by the caller. */
#include "native_install_state.h"
#include <string.h>
static int hex(const char *p,unsigned int n)
{ unsigned int i;for(i=0;i<n;++i)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return 0;return 1; }
static int text(const char *p,unsigned int n) { return p[0] && memchr(p,0,n)!=NULL; }
int prox_ni_job(const ProxNativeInstallJob *job)
{
    const char *component;unsigned int i,n;size_t base=sizeof("ms0:/PSP/GAME/PROXUPDATE/")-1;
    if(!job || !text(job->root,sizeof(job->root)) || !text(job->source_version,40) || !text(job->target_version,40) ||
       !text(job->build_id,32) || strlen(job->build_id)!=18 || strncmp(job->build_id,"pro-x-",6) || !hex(job->build_id+6,12) ||
       !memchr(job->manifest_sha256,0,65) || strlen(job->manifest_sha256)!=64 || !hex(job->manifest_sha256,64) || job->model>=16) return -1;
    if(strlen(job->root)!=base+34 || (strncmp(job->root,"ms0:/PSP/GAME/PROXUPDATE/",base) &&
       strncmp(job->root,"ef0:/PSP/GAME/PROXUPDATE/",base))) return -1;
    component=job->root+base;
    if(strncmp(component,".stage-",7) || strncmp(component+7,job->build_id,18) || component[25]!='-' || !hex(component+26,8)) return -1;
    for(i=0;i<PROX_NI_PARTS;++i) {
        n=job->parts[i].size;
        if(!n || n>1024u*1024u || !memchr(job->parts[i].sha256,0,65) ||
           strlen(job->parts[i].sha256)!=64 || !hex(job->parts[i].sha256,64)) return -1;
    }
    return 0;
}
int prox_ni_arm(ProxNativeInstallState *s,int worker,int consumer,unsigned int lease,unsigned int generation,
                const char *target,const ProxNativeInstallJob *job,unsigned int now)
{
    if(worker<=0 || consumer<=0 || !lease || !generation || !target || prox_ni_job(job) ||
       strcmp(target,job->target_version) || prox_ni_locked(s)) return -1;
    memset(s,0,sizeof(*s));s->lease=lease;s->generation=generation;s->worker=worker;s->consumer=consumer;
    s->phase=PROX_NI_PREPARED;s->job=*job;s->last_seen=now;return 0;
}
int prox_ni_exchange(ProxNativeInstallState *s,int caller,int action,int cancel,ProxNativeInstallMessage *m,unsigned int now)
{
    if(!m || m->protocol!=PROX_NI_PROTOCOL || m->lease!=s->lease || m->generation!=s->generation || !s->phase || caller<=0) return -1;
    if(action==PROX_NI_BIND) {
        if(caller!=s->worker || s->phase!=PROX_NI_PREPARED || s->module || m->module<=0 || cancel) return -1;
        s->module=m->module;return 0;
    }
    if(action==PROX_NI_CLAIM) {
        if(cancel || s->phase!=PROX_NI_PREPARED || !s->module || m->module!=s->module ||
           caller==s->worker || caller==s->consumer || s->engine) return -1;
        s->engine=caller;s->phase=PROX_NI_CLAIMED;s->last_seen=now;m->job=s->job;return 0;
    }
    if(caller!=s->worker && caller!=s->engine) return -1;
    if(action==PROX_NI_STATUS) {
        m->phase=s->phase;m->cancel=(unsigned int)(cancel && !prox_ni_locked(s));m->result=s->engine;
        m->idle_us=(unsigned int)(now-s->last_seen);
        if(caller==s->engine)s->last_seen=now;
        return 0;
    }
    if(action==PROX_NI_FAIL) {
        if(caller==s->worker && s->phase>=PROX_NI_WRITING) return -1;
        s->phase=PROX_NI_FAILED;return 0;
    }
    if(caller!=s->engine || cancel || s->phase==PROX_NI_FAILED) return -1;
    if(action==PROX_NI_WRITE && s->phase==PROX_NI_CLAIMED) { s->phase=PROX_NI_WRITING;s->last_seen=now;return 0; }
    if(action==PROX_NI_COMMIT && s->phase==PROX_NI_WRITING) { s->phase=PROX_NI_COMMITTED;s->last_seen=now;return 0; }
    return -1;
}
int prox_ni_locked(const ProxNativeInstallState *s)
{ return s->phase==PROX_NI_WRITING || s->phase==PROX_NI_COMMITTED; }
