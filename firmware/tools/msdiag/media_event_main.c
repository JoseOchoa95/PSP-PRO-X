/* GPL-3.0-or-later */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <pspmscm.h>
#include <systemctrl.h>

#include "media_session.h"

PSP_MODULE_INFO("ProxMsMediaEvents", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

struct EventProbe {
    enum MsMediaSession session;
    unsigned insertions, ejections, batches, other;
    unsigned baseline_batches;
    int presence, last_event, last_count;
    int monitor_status, mark_refused;
    int cleanup_status, exit_status;
};

/* The main thread creates the callback and services it between observations. */
static int media_event(int count, int event, void *context)
{
    struct EventProbe *probe = context;
    ++probe->batches;
    if (event == MS_CB_EVENT_INSERTED)
        ++probe->insertions;
    else if (event == MS_CB_EVENT_EJECTED)
        ++probe->ejections;
    else
        ++probe->other;
    probe->last_event = event;
    probe->last_count = count;
    ms_session_notify(&probe->session);
    return 0;
}

static void show(const struct EventProbe *probe)
{
    static const char *const states[] = { "UNMARKED", "CURRENT", "INVALIDATED" };
    pspDebugScreenClear();
    pspDebugScreenPrintf("PRO-X Memory Stick Change Probe\n%s\n\n", MSDIAG_BUILD_ID);
    pspDebugScreenPrintf("Callback registration: 0x%08x\n", (unsigned)probe->monitor_status);
    pspDebugScreenPrintf("Slot: %s (0x%08x)\n", probe->presence == 1 ? "present" :
                         probe->presence == 0 ? "empty" : "query error", (unsigned)probe->presence);
    pspDebugScreenPrintf("Callbacks: insert %u, eject %u, other %u\n", probe->insertions,
                         probe->ejections, probe->other);
    pspDebugScreenPrintf("Last callback: event %d, count %d\n", probe->last_event, probe->last_count);
    pspDebugScreenPrintf("Review: %s\n", states[probe->session]);
    if (probe->session != MS_SESSION_UNCHECKED)
        pspDebugScreenPrintf("Callback batches since mark: %u\n", probe->batches - probe->baseline_batches);
    if (probe->mark_refused)
        pspDebugScreenPrintf("Mark refused: monitoring and card required.\n");
    if (probe->cleanup_status < 0 || probe->exit_status < 0)
        pspDebugScreenPrintf("Exit: cleanup %08x, VSH %08x\n", (unsigned)probe->cleanup_status,
                             (unsigned)probe->exit_status);
    pspDebugScreenPrintf("\nWait for the Memory Stick light before swapping.\nX: mark review    O: exit\n");
}

static int stop_monitor(SceUID *callback, int *registered)
{
    if (*registered) {
        int status = MScmUnregisterMSInsertEjectCallback(*callback);
        if (status < 0)
            return status;
        *registered = 0;
    }
    if (*callback < 0)
        return 0;
    int status = sceKernelDeleteCallback(*callback);
    if (status >= 0)
        *callback = -1;
    return status;
}

int main(void)
{
    struct EventProbe probe = { 0 };
    pspDebugScreenInit();
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    SceUID callback = sceKernelCreateCallback("PRO-X MS change", media_event, &probe);
    probe.monitor_status = callback < 0 ? callback : MScmRegisterMSInsertEjectCallback(callback);
    int registered = probe.monitor_status == 0;
    unsigned previous = 0;
    int redraw = 1;
    for (;;) {
        unsigned batches = probe.batches;
        sceKernelCheckCallback();
        int presence = MScmIsMediumInserted();
        sceKernelCheckCallback();
        if (presence != probe.presence || batches != probe.batches)
            redraw = 1;
        ms_session_observe(&probe.session, presence);
        probe.presence = presence;

        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) {
            probe.cleanup_status = stop_monitor(&callback, &registered);
            if (probe.cleanup_status >= 0) {
                ms_session_notify(&probe.session);
                probe.exit_status = sctrlKernelExitVSH(NULL);
                if (probe.exit_status >= 0)
                    return 0;
            }
            redraw = 1;
        }
        if (pressed & PSP_CTRL_CROSS) {
            int marked = ms_session_mark(&probe.session, registered, probe.presence);
            if (marked)
                probe.baseline_batches = probe.batches;
            probe.mark_refused = !marked;
            redraw = 1;
        }
        if (redraw) {
            show(&probe);
            redraw = 0;
        }
        sceKernelDelayThread(20000);
    }
}
