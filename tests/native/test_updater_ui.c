/* Executes the production selector, with frame-stable PSP controller/network
 * adapters. Multiple reads within one frame see the same held buttons, as on
 * hardware. This catches consumed edges across cancellation/selection polls. */
#include <stdio.h>
static FILE *ui_report_open(const char *,const char *);
#define fopen ui_report_open
#define PROX_UPDATER_UI_NATIVE
#define main prox_updater_program_main
#ifndef PROX_TEST_UPDATER_SOURCE
#define PROX_TEST_UPDATER_SOURCE "../../firmware/Updater/main.c"
#endif
#include PROX_TEST_UPDATER_SOURCE
#undef main
#undef fopen
#include <assert.h>

static unsigned int frames[16], frame, ticks, selector_reads[16];
static int connections, connected_profile, ip_ready, controller_error, no_sample;
static int no_profiles, home_frame;
static int main_mode,close_error,close_calls,launch_calls,remove_calls;
static int latest_profile,latest_rc;
static char output[8192];
static FILE *ui_report_open(const char *path,const char *mode)
{ (void)path; (void)mode; return tmpfile(); }

unsigned int sceKernelGetSystemTimeLow(void) { return ticks; }
int sceKernelDelayThread(unsigned int delay) { ticks += delay; ++frame; return 0; }
int sceCtrlPeekBufferPositive(SceCtrlData *pad, int count)
{
    assert(count == 1);
    if (!connections && frame < 16) ++selector_reads[frame];
    if (home_frame >= 0 && frame == (unsigned int)home_frame) exiting = 1;
    pad->Buttons = frame < 16 ? frames[frame] : 0;
    return controller_error ? (int)0x8002013a : no_sample ? 0 : 1;
}
int pspDebugScreenInit(void) { return 0; }
int pspDebugScreenPrintf(const char *format, ...)
{
    int n;
    size_t used = strlen(output);
    va_list args;
    va_start(args,format); n = vsnprintf(output+used,sizeof(output)-used,format,args); va_end(args);
    return n;
}
int sceUtilityCheckNetParam(int id) { return !no_profiles && (id == 2 || id == 5) ? 0 : -1; }
int sceUtilityGetNetParam(int id, int option, netData *name)
{ (void)id; (void)option; strcpy(name->asString,"fixture profile"); return 0; }
int sceNetApctlConnect(int profile) { ++connections; connected_profile = profile; return 0; }
int sceNetApctlGetState(int *state) { *state = ip_ready ? PSP_NET_APCTL_STATE_GOT_IP : 1; return 0; }
int proxUtilityGetLatestProfile(int *id) { *id=latest_profile; return latest_rc; }
int prox_network_begin(void) { return 0; }
int prox_network_end(void) { return prox_network_close(1,1,1,1,1,1); }
int prox_network_connect(int id)
{
    unsigned int start=ticks;
    sceNetApctlConnect(id);
    while (!prox_stopping() && ticks-start<45000000u) {
        if (ip_ready) { prox_log("IP ready"); return 0; }
        sceKernelDelayThread(20000);
    }
    return -1;
}
int sceKernelCreateCallback(const char *n, int (*f)(int,int,void *), void *a)
{ (void)n; (void)f; (void)a; return 1; }
int sceKernelRegisterExitCallback(int c) { (void)c; return 0; }
int sceKernelSleepThreadCB(void) { return 0; }
int sceKernelCreateThread(const char *n, int (*f)(SceSize,void *), int p, int s, int a, void *o)
{ (void)n; (void)f; (void)p; (void)s; (void)a; (void)o; return 1; }
int sceKernelStartThread(int t, unsigned int n, void *a) { (void)t; (void)n; (void)a; return 0; }
int sceUtilityLoadNetModule(int n) { (void)n; return 0; }
int sceUtilityUnloadNetModule(int n) { (void)n; return 0; }
int sceNetInit(int a,int b,int c,int d,int e) { (void)a; (void)b; (void)c; (void)d; (void)e; return 0; }
int sceNetTerm(void) { return 0; }
int sceNetInetInit(void) { return 0; }
int sceNetInetTerm(void) { return 0; }
int sceNetApctlInit(int s,int p) { (void)s; (void)p; return 0; }
int sceNetApctlTerm(void) { return 0; }
int sceNetApctlDisconnect(void) { return 0; }
int sceNetResolverInit(void) { return 0; }
int sceNetResolverTerm(void) { return 0; }
int sceIoGetstat(const char *p,SceIoStat *s) { (void)p; (void)s; return -1; }
int sctrlKernelExitVSH(void *p) { (void)p; return 0; }
int sceKernelExitGame(void) { return 0; }
int proxUpdateGetState(int s)
{ static const int state[]={1,0,3,0,0x06060110,1}; return main_mode && s >= 0 && s < 6 ? state[s] : 0; }
int prox_https(const char *u,const char *c,size_t n,ProxSink s,void *a,ProxResponse *r)
{ (void)u; (void)c; (void)n; (void)s; (void)a; (void)r; return -1; }
int prox_prepare_update(const char *d,const char *c,const unsigned int v[3],unsigned int m,ProxJob *j)
{ (void)d; (void)c; (void)v; (void)m; strcpy(j->root,"/isolated/stage"); return 1; }
int prox_launch_job(const ProxJob *j) { (void)j; ++launch_calls; return -1; }
int prox_power_ready(void) { return 1; }
int prox_network_close(int r,int a,int i,int n,int m,int c)
{ assert(r && a && i && n && m && c); ++close_calls; return close_error ? -1 : 0; }
void prox_stage_remove(const char *r) { (void)r; ++remove_calls; }
void prox_observe_install(const char *d,const unsigned int v[3]) { (void)d; (void)v; }

static void reset(void)
{
    memset(frames,0,sizeof(frames)); memset(selector_reads,0,sizeof(selector_reads));
    memset(output,0,sizeof(output));
    frame = ticks = 0; connections = connected_profile = 0; ip_ready = 1;
    controller_error = no_sample = no_profiles = 0; home_frame = -1;
    exiting = 0; previous_buttons = ~0u;
    main_mode=close_error=close_calls=launch_calls=remove_calls=0;
    latest_profile=-1; latest_rc=-1;
}

int main(void)
{
    unsigned int i;
    reset(); frames[1] = PSP_CTRL_CROSS;
    assert(connect_network() == 0); /* old double polling fails here */
    assert(connections == 1 && connected_profile == 2);
    for(i=0;i<=1;++i) assert(selector_reads[i] == 1);
    assert(strstr(output,"IP ready"));

    reset(); frames[1]=frames[2]=PSP_CTRL_DOWN; frames[4]=PSP_CTRL_CROSS;
    assert(!connect_network() && connections == 1 && connected_profile == 5);
    for(i=0;i<=4;++i) assert(selector_reads[i] == 1); /* no repeat while held */

    reset(); frames[1]=PSP_CTRL_UP; frames[3]=PSP_CTRL_CROSS;
    assert(!connect_network() && connections == 1 && connected_profile == 5); /* wrap upward */

    reset(); frames[0]=PSP_CTRL_CROSS; frames[2]=PSP_CTRL_CROSS;
    assert(!connect_network() && connections == 1); /* launch held X must be released first */

    reset(); frames[1]=PSP_CTRL_CIRCLE|PSP_CTRL_CROSS;
    assert(connect_network()<0 && exiting && !connections);
    reset(); home_frame=1;
    assert(connect_network()<0 && exiting && !connections);
    reset(); no_profiles=1;
    assert(connect_network()<0 && !connections);
    reset();
    assert(connect_network()<0 && !connections && ticks >= 120000000u);
    reset(); controller_error=1;
    assert(connect_network()<0 && !connections && strstr(output,"Controller input failed"));
    reset(); no_sample=1; previous_buttons=PSP_CTRL_CROSS;
    assert(!buttons() && previous_buttons==PSP_CTRL_CROSS);

    reset(); ip_ready=0; frames[1]=PSP_CTRL_CROSS; frames[2]=PSP_CTRL_CIRCLE;
    assert(connect_network()<0 && exiting && connections==1);
    reset(); latest_profile=5; latest_rc=0;
    assert(!connect_network() && connections == 1 && connected_profile == 5 && frame == 0);
    assert(strstr(output,"Reusing last PSP network profile") && !strstr(output,"Up/Down select"));
    reset(); latest_profile=99; latest_rc=0; frames[1]=PSP_CTRL_CROSS;
    assert(!connect_network() && connected_profile == 2); /* deleted/invalid latest -> selector */
    reset(); latest_profile=0; latest_rc=0; frames[1]=PSP_CTRL_CROSS;
    assert(!connect_network() && connected_profile == 2); /* never guess temporary profile 0 */
    reset(); frames[0]=PSP_CTRL_CROSS; frames[2]=PSP_CTRL_CROSS;
    assert(prox_confirm("fixture confirmation") == 1 && frame == 2);
    reset(); frames[1]=PSP_CTRL_CIRCLE|PSP_CTRL_CROSS;
    assert(!prox_confirm("fixture cancellation") && exiting);
    {
        char *args[]={"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP"};
        reset(); main_mode=1; close_error=1; frames[1]=PSP_CTRL_CROSS; frames[3]=PSP_CTRL_CIRCLE;
        assert(!prox_updater_program_main(1,args));
        assert(close_calls == 1 && !launch_calls && remove_calls == 1);
        assert(strstr(output,"handoff cancelled because network teardown did not complete"));
        reset(); main_mode=1; frames[1]=PSP_CTRL_CROSS; frames[3]=PSP_CTRL_CIRCLE;
        assert(!prox_updater_program_main(1,args));
        assert(close_calls == 1 && launch_calls == 1 && remove_calls == 1);
    }
    puts("PASS: production Wi-Fi/confirmation input and full-main installer handoff gated on completed network teardown");
    return 0;
}
