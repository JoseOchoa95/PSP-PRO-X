/* Diagnostic-only completion state. The source worker owns arm/generation;
 * the verified no-flash module requests completion; native PAF observes text. */
#include <pspkernel.h>
#include <pspsdk.h>
#include "native_completion_preview.h"
#include "native_query_provider.h"
#include "prox_install_preview.h"
#include "prox_native_install.h"
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
static ProxCompletionState notice;
static unsigned short *user_text;
static int guarded;
void prox_native_completion_buffer(unsigned short *buffer) { user_text=buffer; }
void prox_native_completion_reset(void) { memset(&notice,0,sizeof(notice)); }
void prox_native_completion_guard(int ready) { guarded=ready==1; }
int prox_native_completion_arm(unsigned int generation,const unsigned short *text,unsigned int count)
{ return guarded?prox_completion_arm(&notice,generation,user_text,text,count):-1; }
int prox_native_completion_ready(unsigned int generation)
{ return guarded && generation && generation==notice.generation && notice.ready && !notice.failed; }
int prox_native_completion_armed(unsigned int generation)
{ return guarded && user_text && generation && generation==notice.generation && !notice.failed; }
int prox_native_completion_health(unsigned int generation,unsigned int now)
{ return guarded?prox_completion_health(&notice,generation,now):-1; }
void prox_native_completion_buttons(unsigned int buttons)
{
    unsigned int generation=prox_native_completion_generation(),mask=pspSdkDisableInterrupts();
    if(guarded) prox_completion_buttons(&notice,generation,buttons);
    pspSdkEnableInterrupts(mask);
}
int proxNativeCompletionControl(int action)
{
    unsigned int mask,generation=prox_native_completion_generation(),now=sceKernelGetSystemTimeLow(); int thread=sceKernelGetThreadId(),rc=-1;
    if(!generation || !guarded) return -1;
#ifdef PROX_NATIVE_BACKGROUND
    if(!prox_native_install_completion_owner(thread,generation)) return -1;
#else
    if(!sceKernelFindModuleByName(PROX_INSTALL_PREVIEW_MODULE)) return -1;
#endif
    mask=pspSdkDisableInterrupts();
    if(action==PROX_COMPLETION_FINISH) rc=prox_completion_finish(&notice,generation,thread);
    if(action==PROX_COMPLETION_POLL) rc=prox_completion_poll(&notice,generation,thread);
    if(action==PROX_COMPLETION_RETRY)rc=prox_completion_retry(&notice,generation,thread);
    if(action==PROX_COMPLETION_FAIL && notice.generation==generation && (!notice.owner || notice.owner==thread)) {
        notice.failed=1;rc=0;
    }
    if(rc>=0 && (action==PROX_COMPLETION_FINISH || action==PROX_COMPLETION_POLL || action==PROX_COMPLETION_RETRY)) notice.last_poll=now;
    pspSdkEnableInterrupts(mask);return rc;
}
const unsigned short *proxNativeCompletionText(const char *key)
{
    unsigned int mask,generation,address=(unsigned int)key,i;int k1;const unsigned short *result=NULL;
    if(!key || address<0x08800000u || address>0x0c000000u-64) return NULL;
    generation=prox_native_completion_generation(); if(!generation) return NULL;
    k1=pspSdkSetK1(0);
    for(i=0;i<64;++i) if(!key[i]) break;
    mask=pspSdkDisableInterrupts();
    if(i<64 && guarded && user_text && prox_completion_select(&notice,generation,key)) result=user_text;
    pspSdkEnableInterrupts(mask);pspSdkSetK1(k1);return result;
}
int proxNativeCompletionBack(int visible)
{
    unsigned int generation=prox_native_completion_generation(),mask=pspSdkDisableInterrupts();
    int result=guarded?prox_completion_back(&notice,generation,visible):visible;
    pspSdkEnableInterrupts(mask);return result;
}
#else
void prox_native_completion_buffer(unsigned short *p) { (void)p; }
void prox_native_completion_reset(void) { }
void prox_native_completion_guard(int r) { (void)r; }
int prox_native_completion_arm(unsigned int g,const unsigned short *p,unsigned int n) { (void)g;(void)p;(void)n;return -1; }
int prox_native_completion_ready(unsigned int g) { (void)g;return 0; }
int prox_native_completion_armed(unsigned int g) { (void)g;return 0; }
int prox_native_completion_health(unsigned int g,unsigned int now) { (void)g;(void)now;return -1; }
void prox_native_completion_buttons(unsigned int buttons) { (void)buttons; }
int proxNativeCompletionControl(int action) { (void)action;return -1; }
const unsigned short *proxNativeCompletionText(const char *key) { (void)key;return NULL; }
int proxNativeCompletionBack(int visible) { return visible; }
#endif
