#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_install_state.h"
int main(void)
{
    ProxNativeInstallState state={0};ProxNativeInstallMessage m={0};ProxNativeInstallJob job={0};unsigned int i;
    strcpy(job.root,"ms0:/PSP/GAME/PROXUPDATE/.stage-pro-x-123456789abc-00112233");
    strcpy(job.build_id,"pro-x-123456789abc");strcpy(job.source_version,"0.3.2");strcpy(job.target_version,"0.3.3");
    memset(job.manifest_sha256,'a',64);job.model=1;
    for(i=0;i<11;++i) { job.parts[i].size=512;memset(job.parts[i].sha256,'b',64); }
#ifdef PROX_NATIVE_DEDICATED_OTA
    strcpy(job.root,"ms0:/PROXDIAG/.dedicated-0000004d-00000004");
    m.job=job; m.lease=77; m.generation=4;
    m.worker.size=600000;m.package.size=900000;
    memset(m.worker.sha256,'c',64);memset(m.package.sha256,'d',64);
    assert(prox_ni_arm_dedicated(&state,2,1,"0.3.3",900001,&m,100)<0);
    ++m.generation;assert(prox_ni_arm_dedicated(&state,2,1,"0.3.3",900000,&m,100)<0);--m.generation;
    assert(prox_ni_arm_dedicated(&state,2,1,"0.3.4",900000,&m,100)<0);
    assert(!prox_ni_arm_dedicated(&state,2,1,"0.3.3",900000,&m,100));
    memset(&m,0,sizeof(m));
#else
    assert(!prox_ni_job(&job));assert(prox_ni_arm(&state,2,1,77,4,"0.3.4",&job,100)<0);
    assert(!prox_ni_arm(&state,2,1,77,4,"0.3.3",&job,100));
#endif
    m.protocol=PROX_NI_PROTOCOL;m.lease=77;m.generation=4;m.module=17;
    assert(prox_ni_exchange(&state,1,PROX_NI_BIND,0,&m,101)<0);
    assert(!prox_ni_exchange(&state,2,PROX_NI_BIND,0,&m,101));
    assert(prox_ni_exchange(&state,2,PROX_NI_CLAIM,0,&m,102)<0);
    assert(prox_ni_exchange(&state,1,PROX_NI_CLAIM,0,&m,102)<0);
    assert(!prox_ni_exchange(&state,3,PROX_NI_CLAIM,0,&m,102) && !memcmp(&m.job,&job,sizeof(job)));
#ifdef PROX_NATIVE_DEDICATED_OTA
    assert(m.worker.size==600000 && m.package.size==900000 && m.worker.sha256[63]=='c' && m.package.sha256[63]=='d');
#endif
    assert(prox_ni_exchange(&state,2,PROX_NI_WRITE,0,&m,103)<0);
    assert(prox_ni_exchange(&state,3,PROX_NI_COMMIT,0,&m,103)<0);
    assert(prox_ni_exchange(&state,3,PROX_NI_WRITE,1,&m,103)<0 && !prox_ni_locked(&state));
    assert(!prox_ni_exchange(&state,3,PROX_NI_WRITE,0,&m,103) && prox_ni_locked(&state));
    assert(prox_ni_exchange(&state,2,PROX_NI_FAIL,0,&m,104)<0);
    assert(prox_ni_arm(&state,2,1,77,5,"0.3.3",&job,104)<0);
    assert(!prox_ni_exchange(&state,3,PROX_NI_STATUS,1,&m,105) && !m.cancel);
    assert(!prox_ni_exchange(&state,3,PROX_NI_COMMIT,0,&m,106));
    assert(state.phase==PROX_NI_COMMITTED && prox_ni_locked(&state));
    ++m.generation;assert(prox_ni_exchange(&state,3,PROX_NI_FAIL,0,&m,107)<0);--m.generation;
    assert(!prox_ni_exchange(&state,3,PROX_NI_FAIL,0,&m,108) && !prox_ni_locked(&state));
    job.root[0]='x';assert(prox_ni_job(&job)<0);
    puts("PASS: copied exact target job, worker/consumer/engine roles, stale generations, pre-write cancel, write latch and committed-only success; no flash capability");
}
