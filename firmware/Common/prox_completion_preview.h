/* Private no-flash completion diagnostic; not an installation-success ABI. */
#ifndef PROX_COMPLETION_PREVIEW_H
#define PROX_COMPLETION_PREVIEW_H
#include <string.h>
#define PROX_COMPLETION_UNITS 120u
#define PROX_COMPLETION_RCO_SIZE 377448u
#define PROX_COMPLETION_RCO_SHA256 "714c0af3c51f61a895311ee1e58e719d8ed7fc5d3e5e441de38d67133617c265"
enum { PROX_COMPLETION_FINISH=1,PROX_COMPLETION_POLL=2,PROX_COMPLETION_FAIL=3,PROX_COMPLETION_RETRY=4 };
#define PROX_COMPLETION_BUTTONS 0x4000u /* PSP Cross only */
#define PROX_COMPLETION_LIVE_US 25000000u
int proxNativeCompletionControl(int action);
const unsigned short *proxNativeCompletionText(const char *key);
int proxNativeCompletionBack(int visible);
typedef struct { unsigned int generation,last_poll; int owner,ready,shown,released,restart,failed; } ProxCompletionState;
static inline int prox_completion_arm(ProxCompletionState *s,unsigned int generation,
                                      unsigned short *out,const unsigned short *text,unsigned int count)
{
    unsigned int i;
    if(!generation || !out || !text || count<2 || count>PROX_COMPLETION_UNITS || !text[0] || text[count-1]) return -1;
    for(i=0;i+1<count;++i) if(!text[i] || text[i]=='\r' || text[i]=='\n') return -1;
    memcpy(out,text,count*sizeof(*out));
    memset(s,0,sizeof(*s)); s->generation=generation; return 0;
}
static inline int prox_completion_finish(ProxCompletionState *s,unsigned int generation,int thread)
{
    if(!generation || s->generation!=generation || thread<=0 || s->ready || s->owner || s->failed) return -1;
    s->owner=thread; s->ready=1; return 0;
}
static inline int prox_completion_poll(const ProxCompletionState *s,unsigned int generation,int thread)
{
    return !generation || s->generation!=generation || thread<=0 || thread!=s->owner || !s->ready || s->failed?-1:
           s->shown?(s->restart?2:1):0;
}
static inline int prox_completion_select(ProxCompletionState *s,unsigned int generation,const char *key)
{
    if(!generation || s->generation!=generation || !s->ready || s->failed || !key ||
       strcmp(key,"msgupdate_downloaded_system_update")) return 0;
    s->shown=1; return 1;
}
static inline void prox_completion_buttons(ProxCompletionState *s,unsigned int generation,unsigned int buttons)
{
    if(!generation || s->generation!=generation || !s->ready || !s->shown || s->failed) return;
    if(!(buttons&PROX_COMPLETION_BUTTONS)) s->released=1;
    else if(s->released) s->restart=1;
}
static inline int prox_completion_health(const ProxCompletionState *s,unsigned int generation,unsigned int now)
{
    if(!generation || s->generation!=generation || s->failed) return -1;
    if(!s->owner) return 0; /* Worker separately bounds module startup. */
    if((unsigned int)(now-s->last_poll)>=PROX_COMPLETION_LIVE_US) return -1;
    return s->shown?2:1;
}
static inline int prox_completion_back(const ProxCompletionState *s,unsigned int generation,int visible)
{
    return generation && s->generation==generation && s->ready && s->shown && !s->failed?0:visible;
}
static inline int prox_completion_retry(ProxCompletionState *s,unsigned int generation,int thread)
{
    if(prox_completion_poll(s,generation,thread)!=2)return -1;
    s->restart=0;s->released=0;return 0;
}
#endif
