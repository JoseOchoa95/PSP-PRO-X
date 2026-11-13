/* PRO-X diagnostic, GPL-3.0-or-later. Never connects or patches a Sony module.
 * Run WITHOUT external WPA2 plugins. Output is a PRIVATE identification report,
 * not an approved signature. See docs/wpa2-support.md before sharing it. */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <psputilsforkernel.h>
#include <stdio.h>
#include <string.h>
#include "systemctrl.h"
#include "wpa2_patch.h"

PSP_MODULE_INFO("ProxWpa2Diag",PSP_MODULE_KERNEL,1,0);
PSP_MAIN_THREAD_ATTR(0);
PSP_MAIN_THREAD_STACK_SIZE_KB(64);
static STMOD_HANDLER previous;
static Wpa2Module captured;
static unsigned char original[WPA2_SITE_COUNT][WPA2_MAX_BYTES];
static int valid[WPA2_SITE_COUNT], seen;
static unsigned char text_sha1[20];
static int hash_result = -1;
static int after_handler_wpa2_sites;
/* Candidates to compare across captures, NOT approved invariant signatures. */
static const unsigned int guard_offset[] = {0x100,0x200,0xeef0,0xf440};
static unsigned char guard_bytes[4][32];
static int guard_valid[4];

static int capture(SceModule2 *mod)
{
    unsigned int i;
    int captured_now = 0, result;
    if (!strcmp(mod->modname,"sceNetApctl_Library") && !seen) {
        captured.name = "sceNetApctl_Library";
        captured.firmware = sceKernelDevkitVersion();
        captured.text = mod->text_addr;
        captured.text_size = mod->text_size;
        captured.data_size = mod->data_size;
        captured.bss_size = mod->bss_size;
        captured.nsegment = mod->nsegment;
        for (i = 0; i < 4; ++i) {
            captured.segment[i].address = mod->segmentaddr[i];
            captured.segment[i].size = mod->segmentsize[i];
        }
        /* Bounded pre-start snapshot only; no I/O, profile access or network. */
        if (captured.text_size <= 1024*1024 &&
            prox_wpa2_range(&captured,0,captured.text_size,4,1))
            hash_result = sceKernelUtilsSha1Digest((void *)captured.text,
                                                 captured.text_size,text_sha1);
        for (i = 0; i < WPA2_SITE_COUNT; ++i) {
            const Wpa2Site *s = &prox_wpa2_sites[i];
            valid[i] = prox_wpa2_range(&captured,s->offset,s->size,s->alignment,s->code);
            if (valid[i]) memcpy(original[i],(void *)(captured.text+s->offset),s->size);
        }
        for (i = 0; i < 4; ++i) {
            guard_valid[i] = prox_wpa2_range(&captured,guard_offset[i],32,4,1);
            if (guard_valid[i]) memcpy(guard_bytes[i],(void *)(captured.text+guard_offset[i]),32);
        }
        seen = 1;
        captured_now = 1;
    }
    result = previous ? previous(mod) : 0;
    if (captured_now) {
        unsigned char replacement[WPA2_MAX_BYTES];
        for (i = 0; i < WPA2_SITE_COUNT; ++i) {
            const Wpa2Site *s = &prox_wpa2_sites[i];
            if (!valid[i]) continue;
            prox_wpa2_replacement(&captured,i,replacement);
            if (!memcmp((void *)(captured.text+s->offset),replacement,s->size))
                ++after_handler_wpa2_sites;
        }
    }
    return result;
}

int main(int argc, char **argv)
{
    int common, inet, i, j, control_result, exit_result;
    unsigned int start;
    FILE *out;
    SceCtrlData pad;
    pspDebugScreenInit();
    pspDebugScreenPrintf("PRO-X WPA2 identification only\nNo network connection or patch.\n");
    previous = sctrlHENSetStartModuleHandler(capture);
    common = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    inet = common >= 0 ? sceUtilityLoadNetModule(PSP_NET_MODULE_INET) : -1;
    sctrlHENSetStartModuleHandler(previous);
    out = fopen("ms0:/PSP/GAME/WPA2DIAG/report.txt","w");
    if (!out) out = fopen("ef0:/PSP/GAME/WPA2DIAG/report.txt","w");
    if (out) {
        fprintf(out,"PRO-X WPA2 diagnostic v2; identification only, no network test\n");
        fprintf(out,"net_common=%08x net_inet=%08x captured=%d\n",common,inet,seen);
        fprintf(out,"after_previous_handler_wpa2_sites=%d/%d (payload bytes only; not handshake evidence)\n",
                after_handler_wpa2_sites,WPA2_SITE_COUNT);
        fprintf(out,"firmware=%08x hen=%08x base=%08x text=%x data=%x bss=%x segments=%u\n",
                captured.firmware,sctrlHENGetVersion(),(unsigned)captured.text,
                (unsigned)captured.text_size,(unsigned)captured.data_size,
                (unsigned)captured.bss_size,captured.nsegment);
        for (i = 0; i < captured.nsegment && i < 4; ++i)
            fprintf(out,"segment[%d]=%08x,%x\n",i,(unsigned)captured.segment[i].address,
                    (unsigned)captured.segment[i].size);
        fprintf(out,"relocated_text_sha1_status=%d sha1=",hash_result);
        for (i=0;i<20;++i) fprintf(out,"%02x",text_sha1[i]);
        fprintf(out,"\n");
        for (i=0;i<WPA2_SITE_COUNT && seen;++i) {
            fprintf(out,"site=%05x width=%u valid=%d bytes=",prox_wpa2_sites[i].offset,
                    prox_wpa2_sites[i].size,valid[i]);
            if (valid[i]) for(j=0;j<prox_wpa2_sites[i].size;++j) fprintf(out,"%02x",original[i][j]);
            fprintf(out,"\n");
        }
        for (i=0;i<4 && seen;++i) {
            fprintf(out,"candidate_guard=%05x valid=%d bytes=",guard_offset[i],guard_valid[i]);
            if (guard_valid[i]) for(j=0;j<32;++j) fprintf(out,"%02x",guard_bytes[i][j]);
            fprintf(out,"\n");
        }
        if (fclose(out) == 0 && seen)
            pspDebugScreenPrintf("Saved report.txt in PSP/GAME/WPA2DIAG.\n");
        else pspDebugScreenPrintf("Report failed or module not captured.\n");
    } else pspDebugScreenPrintf("Cannot save report.\n");
    /* This is a kernel thread: link the kernel controller driver, avoid a
     * blocking read, check errors and always bound the wait after closing I/O. */
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenPrintf("Press X to return, or wait 15 seconds.\n");
    start = sceKernelGetSystemTimeLow();
    do {
        memset(&pad,0,sizeof(pad));
        control_result = sceCtrlPeekBufferPositive(&pad,1);
        if (control_result < 0) {
            pspDebugScreenPrintf("Controller error: %08x\n",control_result);
            break;
        }
        if (control_result > 0 && (pad.Buttons & PSP_CTRL_CROSS)) break;
        sceKernelDelayThread(20000);
    } while ((unsigned int)(sceKernelGetSystemTimeLow()-start) < 15000000u);
    exit_result = sctrlKernelExitVSH(NULL);
    pspDebugScreenPrintf("Return failed: %08x. Report is closed; you may power off.\n",exit_result);
    return 0;
}
