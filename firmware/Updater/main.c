/* PRO-X GitHub system-update client, GPL-3.0-or-later. */
#ifdef PROX_UPDATER_UI_NATIVE
#include "updater_ui_shim.h"
#else
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include "systemctrl.h"
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "prox_product.h"
#include "bridge.h"
#include "transport.h"
#include "update.h"
#include "stage.h"
#include "network_close.h"
#include "network_session.h"

PSP_MODULE_INFO("ProxUpdater", 0, 0, 3);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(128);
PSP_HEAP_SIZE_KB(8192);

static FILE *report;
static volatile int exiting;
static unsigned int previous_buttons = ~0u;
static char directory[256], ca_path[288], report_path[288];

static unsigned int buttons(void)
{
    SceCtrlData pad;
    unsigned int pressed;
    int rc;
    memset(&pad, 0, sizeof(pad));
    rc = sceCtrlPeekBufferPositive(&pad, 1);
    if (rc < 0) { prox_log("Controller input failed rc=0x%08x", rc); exiting = 1; return 0; }
    if (rc == 0) return 0; /* no new sample: retain the previous held state */
    pressed = pad.Buttons & ~previous_buttons;
    previous_buttons = pad.Buttons;
    return pressed;
}

int prox_stopping(void)
{
    if (buttons() & PSP_CTRL_CIRCLE) exiting = 1;
    return exiting;
}

void prox_log(const char *format, ...)
{
    char buffer[384];
    va_list arguments;
    va_start(arguments, format); vsnprintf(buffer, sizeof(buffer), format, arguments); va_end(arguments);
    pspDebugScreenPrintf("%s\n", buffer);
    if (report && (fprintf(report, "%s\n", buffer) < 0 || fflush(report))) exiting = 1;
}

static int exit_callback(int a, int b, void *arg)
{ (void)a; (void)b; (void)arg; exiting = 1; return 0; }

static int callbacks(SceSize size, void *arg)
{
    int callback;
    (void)size; (void)arg;
    callback = sceKernelCreateCallback("PRO-X updater exit", exit_callback, NULL);
    if (callback >= 0) sceKernelRegisterExitCallback(callback);
    sceKernelSleepThreadCB(); return 0;
}

static int connect_network(void)
{
    int profiles[99], count = 0, id, selected = 0, redraw = 1, latest=-1, rc;
    unsigned int start = sceKernelGetSystemTimeLow();
    rc=proxUtilityGetLatestProfile(&latest);
    if (!rc && latest >= 1 && latest <= 99 && !sceUtilityCheckNetParam(latest)) {
        prox_log("Reusing last PSP network profile id=%d; O cancels",latest);
        return prox_network_connect(latest);
    }
    prox_log("Last PSP profile unavailable rc=0x%08x id=%d; choose a saved connection",rc,latest);
    for (id = 1; id <= 99; ++id) if (!sceUtilityCheckNetParam(id)) profiles[count++] = id;
    if (!count) { prox_log("Create a saved Wi-Fi profile in XMB first."); return -1; }
    while (!exiting && (unsigned int)(sceKernelGetSystemTimeLow()-start) < 120000000u) {
        unsigned int pressed;
        if (redraw) {
            netData name;
            char safe[42];
            int i;
            memset(&name, 0, sizeof(name)); memset(safe, 0, sizeof(safe));
            if (!sceUtilityGetNetParam(profiles[selected], PSP_NETPARAM_NAME, &name))
                for (i = 0; i < 40 && name.asString[i]; ++i)
                    safe[i] = name.asString[i] >= 32 && name.asString[i] <= 126 ? name.asString[i] : '?';
            /* Profile names are displayed but never written to the report. */
            pspDebugScreenPrintf("\nWi-Fi %d/%d: %s\nUp/Down select, X connect, O cancel\n", selected+1, count, safe);
            redraw = 0;
        }
        pressed = buttons();
        /* Read once for this interactive tick. Calling prox_stopping() here
         * would consume X/Up/Down edges before the selector could use them. */
        if (pressed & PSP_CTRL_CIRCLE) exiting = 1;
        if (exiting) break;
        if (pressed & PSP_CTRL_UP) { selected = (selected+count-1)%count; redraw = 1; }
        if (pressed & PSP_CTRL_DOWN) { selected = (selected+1)%count; redraw = 1; }
        if (pressed & PSP_CTRL_CROSS) {
            prox_log("Wi-Fi profile confirmed id=%d", profiles[selected]);
            break;
        }
        sceKernelDelayThread(20000);
    }
    if (exiting || (unsigned int)(sceKernelGetSystemTimeLow()-start) >= 120000000u) return -1;
    return prox_network_connect(profiles[selected]);
}

int prox_confirm(const char *prompt)
{
    unsigned int start=sceKernelGetSystemTimeLow();
    prox_log("%s",prompt);
    while (!exiting && (unsigned int)(sceKernelGetSystemTimeLow()-start) < 120000000u) {
        unsigned int pressed=buttons();
        if (pressed & PSP_CTRL_CIRCLE) exiting=1;
        if (exiting) break;
        if (pressed & PSP_CTRL_CROSS) return 1;
        sceKernelDelayThread(20000);
    }
    return 0;
}

int main(int argc, char **argv)
{
    ProxJob job;
    int callback, rc, i;
    unsigned int version[3];
    time_t now;
    memset(&job,0,sizeof(job));
    pspDebugScreenInit();
    if (argc < 1 || strlen(argv[0]) >= sizeof(directory) ||
        (strncmp(argv[0], "ms0:/", 5) && strncmp(argv[0], "ef0:/", 5))) goto exit;
    strcpy(directory, argv[0]);
    if (!strrchr(directory, '/')) goto exit;
    *strrchr(directory, '/') = 0;
    snprintf(ca_path, sizeof(ca_path), "%s/cacert.pem", directory);
    for (i = 1; i <= 999; ++i) {
        SceIoStat stat;
        snprintf(report_path, sizeof(report_path), "%s/report-%03d.txt", directory, i);
        if (sceIoGetstat(report_path, &stat) < 0) { report = fopen(report_path, "wb"); break; }
    }
    if (!report) { pspDebugScreenPrintf("Cannot create private report.\n"); goto exit; }
    callback = sceKernelCreateThread("PRO-X exit", callbacks, 0x11, 0x1000, PSP_THREAD_ATTR_USER, NULL);
    if (callback >= 0) sceKernelStartThread(callback, 0, NULL);
    prox_log("PRO-X v" PROX_PRODUCT_VERSION " updater build " PROX_BUILD_ID);
    prox_log("Stable compatible GitHub releases only; no downgrade or network fallback.");
    if (proxUpdateGetState(0) != 1) { prox_log("Required running PRO-X bridge is unavailable."); goto done; }
    for (i = 0; i < 3; ++i) version[i] = (unsigned int)proxUpdateGetState(i+1);
    prox_log("Running firmware product=%u.%u.%u devkit=0x%08x model_id=%d", version[0], version[1], version[2],
             proxUpdateGetState(4), proxUpdateGetState(5));
    if (proxUpdateGetState(4) != 0x06060110) { prox_log("Only base firmware 6.61 is supported."); goto done; }
    prox_observe_install(directory,version);
    now = time(NULL);
    prox_log("RTC epoch=%ld; correct PSP date/time if certificate dates fail", (long)now);
    if (now < 1735689600) { prox_log("Clock appears incorrect. Correct PSP date/time first."); goto done; }
    if (prox_network_begin()) goto cleanup;
    if (connect_network()) goto cleanup;
    rc=prox_prepare_update(directory,ca_path,version,(unsigned int)proxUpdateGetState(5),&job);
    if (rc < 0) prox_log("Update check/staging failed or cancelled; installed firmware untouched");
cleanup:
    rc=prox_network_end();
    if (rc < 0 && job.root[0]) {
        prox_stage_remove(job.root); job.root[0]=0;
        prox_log("Installer handoff cancelled because network teardown did not complete");
    }
    if (job.root[0] && (exiting || !prox_power_ready())) {
        prox_log("Installer handoff cancelled by final input/power/log check");
        prox_stage_remove(job.root); job.root[0]=0;
    }
    if (job.root[0]) {
        prox_log("Networking closed. Handing verified stage to the PRO-X installer.");
        rc=fclose(report); report=NULL;
        if (!rc) rc=prox_launch_job(&job);
        report=fopen(report_path,"ab");
        prox_log("Installer launch failed rc=0x%08x; no firmware was written",rc);
        prox_stage_remove(job.root);
    }
done:
    prox_log("O returns to XMB. The network client does not write flash or reset preferences.");
    prox_log("Private report: %s", report_path);
    while (!prox_stopping()) sceKernelDelayThread(20000);
    if (report) fclose(report);
    report = NULL;
exit:
    sctrlKernelExitVSH(NULL);
    sceKernelExitGame();
    return 0;
}
