/* GPL-3.0-or-later */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <string.h>
#include <systemctrl.h>

#include "psp_media.h"

PSP_MODULE_INFO("ProxAutoMemoryStick", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

enum { PAGE_SIZE = 8 };
static struct MsAutoMedia device;
static const char *failure;
static SceUID raw_handle = -1;

static int close_raw(void)
{
    if (raw_handle < 0)
        return 1;
    if (sceIoClose(raw_handle) < 0)
        return 0;
    raw_handle = -1;
    return 1;
}

static void run_scan(void)
{
    memset(&device, 0, sizeof(device));
    if (!close_raw()) {
        failure = "The previous read-only handle could not close.";
        return;
    }
    raw_handle = sceIoOpen("msstor:", PSP_O_RDONLY, 0);
    if (raw_handle < 0) {
        failure = "The Memory Stick device could not be opened.";
        return;
    }
    failure = ms_auto_measure(raw_handle, &device);
    if (!close_raw()) {
        failure = "The read-only Memory Stick handle could not close.";
        device.layout = MS_FORMAT_INVALID;
        device.capacity = (struct AutoCapacity){0};
    }
}

static void show(unsigned page, int scanned)
{
    pspDebugScreenClear();
    pspDebugScreenPrintf("PRO-X Memory Stick Auto Preview\n%s\n\n", MSDIAG_BUILD_ID);
    if (!scanned) {
        pspDebugScreenPrintf("Insert the microSD to measure with its adapter.\n"
                             "Wait for the Memory Stick light to go out.\n\nX: scan    O: exit\n");
        return;
    }
    if (device.reported)
        pspDebugScreenPrintf("Adapter: %u sectors\n", device.reported);
    if (failure) {
        pspDebugScreenPrintf("%s\n\nX: rescan    O: exit\n", failure);
        return;
    }
    const char *name = device.capacity.estimate.finding == CAPACITY_CAP_SAMPLE ? "Cap sample" : "Boundary candidate";
    pspDebugScreenPrintf("%s: %u sectors\n", name, device.capacity.estimate.sectors);
    pspDebugScreenPrintf("Distinct sample periods: %u/%u\n", device.capacity.distinct_checks, device.capacity.alias_checks);
    if (device.capacity.distinct_checks < device.capacity.alias_checks)
        pspDebugScreenPrintf("Equal samples are inconclusive for aliasing.\n");
    if (device.gpt)
        pspDebugScreenPrintf("GPT detected. Conversion is pending.\n");
    else if (device.have_snapshot)
        pspDebugScreenPrintf("Live witness LBAs: %u, %u\n", device.snapshot.witnesses[0].lba, device.snapshot.witnesses[1].lba);
    else if (device.layout == MS_FORMAT_EXTENDED)
        pspDebugScreenPrintf("Live witnesses for the write guard: inconclusive.\n");
    if (device.layout == MS_FORMAT_STOCK) {
        pspDebugScreenPrintf("Capacity candidate uses the ordinary Sony path.\n");
    } else if (device.layout == MS_FORMAT_EXTENDED) {
        pspDebugScreenPrintf("\n%u FAT32 volumes (%u primary, %u logical)\n", device.plan.volume_count,
                             device.plan.primary_count, device.plan.volume_count - device.plan.primary_count);
        unsigned first = page * PAGE_SIZE;
        for (unsigned i = first; i < device.plan.volume_count && i < first + PAGE_SIZE; ++i) {
            struct MsFormatVolume volume;
            if (ms_format_volume(&device.plan, i, &volume)) {
                unsigned gib = (uint64_t)volume.sectors * 100 / 2097152;
                pspDebugScreenPrintf("%u. %u.%02u GiB", i + 1, gib / 100, gib % 100);
                pspDebugScreenPrintf("  start %u  sectors %u\n", volume.start, volume.sectors);
            }
        }
    } else {
        pspDebugScreenPrintf("A FAT32 layout could not be calculated.\n");
    }
    pspDebugScreenPrintf("\nRead-only preview. Planning cap: 1 TB.\n"
                         "Left/Right: pages    X: rescan    O: exit\n");
}

int main(void)
{
    pspDebugScreenInit();
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    unsigned previous = 0, page = 0;
    int scanned = 0;
    show(page, scanned);
    for (;;) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) {
            if (close_raw() && sctrlKernelExitVSH(NULL) >= 0)
                return 0;
        } else if (pressed & PSP_CTRL_CROSS) {
            pspDebugScreenClear();
            pspDebugScreenPrintf("Measuring the inserted card. Keep it inserted.\n");
            run_scan();
            scanned = 1;
            page = 0;
            show(page, scanned);
        } else if ((pressed & PSP_CTRL_RIGHT) && (page + 1) * PAGE_SIZE < device.plan.volume_count) {
            show(++page, scanned);
        } else if ((pressed & PSP_CTRL_LEFT) && page) {
            show(--page, scanned);
        }
        sceKernelDelayThread(20000);
    }
}
