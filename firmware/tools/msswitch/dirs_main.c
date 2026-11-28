/* GPL-3.0-or-later */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <systemctrl.h>

#include "fat32_dirs.h"
#include "ram_volume.h"
#ifdef MSDIRS_CARD_TRIAL
#include "dirs_card.h"
static struct Fat32Ram staged_volume;
#endif
#if defined(MSAUTO_FORMAT_TRIAL) && !defined(MSWRITE_PATH_TRIAL)
#include "auto_format.h"
#endif

PSP_MODULE_INFO("ProxRamDirectoryTrial", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

enum { PARTITION_START = 461375488u, PARTITION_SECTORS = 39319552u };

static int run_trial(void)
{
    if (!sony_mkdir_ram_check()) {
        pspDebugScreenPrintf("Sony 6.61 FAT module profile: REJECTED\n");
        return 0;
    }
    pspDebugScreenPrintf("Sony 6.61 FAT module profile: MATCH\n");

    struct MsRamTrial trial;
    struct Fat32Ram *output = NULL;
#ifdef MSDIRS_CARD_TRIAL
    output = &staged_volume;
#endif
    enum MsRamTrialStatus status = ms_ram_volume_test(PARTITION_START, PARTITION_SECTORS, 0x50585031,
                                                      output, &trial);
    if (status == MS_RAM_TRIAL_PREPARE) {
        pspDebugScreenPrintf("Could not prepare the private RAM device.\n");
        return 0;
    }
    if (status == MS_RAM_TRIAL_OPEN) {
        pspDebugScreenPrintf("RAM device open failed: 0x%08x\n", (unsigned)trial.open_error);
        return 0;
    }

    int success = trial.tree && trial.bounds && trial.denied && trial.clean;
    pspDebugScreenPrintf("Sony mkdir: 0x%08x\nDirectories: %u/%u   RAM write calls: %u\n"
                         "Tree, two indicator files and FAT32 summary: %s\nPartition boundary: %s\n"
                         "Denied write propagation: %s\n"
                         "Unmount: 0x%08x  mutex close: 0x%08x\n"
                         "RAM device cleanup: %s\n\nRAM directory trial: %s\n",
                         (unsigned)trial.created.result, trial.created.created, PSP_INITIAL_DIRECTORY_COUNT, trial.writes,
                         !trial.created_all ? "NOT RUN" : trial.tree ? "PASS" : "FAIL",
                         !trial.created_all ? "NOT RUN" : trial.bounds ? "PASS" : "FAIL",
                         !trial.try_denied ? "NOT RUN" : trial.denied ? "PASS" : "FAIL",
                         (unsigned)trial.created.unmount, (unsigned)trial.created.mutex_close,
                         trial.clean ? "PASS" : "FAIL", success ? "PASS" : "FAIL");
    return success;
}

static void heading(void)
{
    pspDebugScreenClear();
#if defined(MSBLANK_TRIAL)
    pspDebugScreenPrintf("PRO-X Memory Stick Uniform Sector Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#elif defined(MSWRITE_PATH_TRIAL)
    pspDebugScreenPrintf("PRO-X Memory Stick Write Path Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#elif defined(MSAUTO_FORMAT_TRIAL)
    pspDebugScreenPrintf("PRO-X Memory Stick Auto Format\n%s\n\n", MSSWITCH_BUILD_ID);
#elif defined(MSMULTI_CARD_TRIAL)
    pspDebugScreenPrintf("PRO-X Memory Stick Whole Card Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#elif defined(MSDIRS_CARD_TRIAL)
    pspDebugScreenPrintf("PRO-X FAT32 Physical Directory Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#else
    pspDebugScreenPrintf("PRO-X FAT32 RAM Directory Trial\n%s\n\n", MSSWITCH_BUILD_ID);
#endif
}

int main(void)
{
    pspDebugScreenInit();
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    heading();
#ifdef MSDIRS_CARD_TRIAL
    enum { PREPARE, CHECK_CARD, CONFIRM, DONE };
#ifdef MSWRITE_PATH_TRIAL
    int stage = CHECK_CARD;
    pspDebugScreenPrintf("Wait for the Memory Stick light to go out.\n"
#ifdef MSBLANK_TRIAL
                         "Insert the backed-up 256 GB card with its original adapter.\n\nX: check card    O: exit\n");
#else
                         "Insert the backed-up 512 GB trial card.\n\nX: check card    O: exit\n");
#endif
#elif defined(MSAUTO_FORMAT_TRIAL)
    int stage = PREPARE;
    if (!dirs_card_start_monitor())
        pspDebugScreenPrintf("Card-change monitor will retry at scan.\n\n");
    pspDebugScreenPrintf("X: check the formatter in RAM\nO: exit\n");
#else
    int stage = dirs_card_load_reference() ? PREPARE : DONE;
    pspDebugScreenPrintf(stage == PREPARE ? "X: prepare the initial filesystem in RAM\nO: exit\n" :
                         "Missing or invalid MSPART.BIN beside EBOOT.PBP.\nO: exit\n");
#endif
#else
    pspDebugScreenPrintf("Sony mkdir on a private FAT32 volume in RAM.\n"
                         "Leave the launch Memory Stick inserted.\n\nX: run trial    O: exit\n");
    int ran = 0;
#endif
    unsigned previous = 0;
    for (;;) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) {
            if (ms_ram_volume_cleanup()
#ifdef MSDIRS_CARD_TRIAL
                && dirs_card_cleanup()
#endif
#if defined(MSAUTO_FORMAT_TRIAL) && !defined(MSWRITE_PATH_TRIAL)
                && dirs_card_stop_monitor()
#endif
            ) {
                int result = sctrlKernelExitVSH(NULL);
                if (result >= 0)
                    return 0;
                pspDebugScreenPrintf("Exit failed: 0x%08x\n", (unsigned)result);
            } else {
                pspDebugScreenPrintf("RAM device cleanup incomplete. Press O to retry.\n");
            }
        }
#ifdef MSDIRS_CARD_TRIAL
#if defined(MSAUTO_FORMAT_TRIAL) && !defined(MSWRITE_PATH_TRIAL)
        else if (stage == CONFIRM && !ms_auto_format_current()) {
            stage = CHECK_CARD;
            heading();
            pspDebugScreenPrintf("Card changed. Format plan discarded.\n\nX: scan again    O: exit\n");
        }
#endif
#if defined(MSMULTI_CARD_TRIAL) && !defined(MSAUTO_FORMAT_TRIAL)
        else if (stage == CHECK_CARD && (pressed & PSP_CTRL_SQUARE)) {
            stage = DONE;
            heading();
            dirs_card_test_mount();
        }
        else if (stage == CHECK_CARD && (pressed & PSP_CTRL_START)) {
            heading();
            if (dirs_card_preview_repeat())
                stage = CONFIRM;
        }
#endif
        else if (pressed & PSP_CTRL_CROSS) {
            if (stage == PREPARE) {
                heading();
                stage = run_trial() ? CHECK_CARD : DONE;
                if (stage == CHECK_CARD)
                    pspDebugScreenPrintf("\nWhen the Memory Stick light is off, insert the\n"
#ifdef MSAUTO_FORMAT_TRIAL
                                         "backed-up microSD with its Memory Stick adapter.\n"
#else
                                         "backed-up 256 GB card with its original adapter.\n"
#endif
                                         "X: check card    O: exit\n"
#if defined(MSMULTI_CARD_TRIAL) && !defined(MSAUTO_FORMAT_TRIAL)
                                         "Square: test mount   Start: repeat format\n"
#endif
                    );
                else
                    pspDebugScreenPrintf("\nO: exit\n");
            } else if (stage == CHECK_CARD) {
                heading();
                if (dirs_card_preview())
                    stage = CONFIRM;
            } else if (stage == CONFIRM &&
                       (pad.Buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) ==
                           (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
                heading();
#if defined(MSAUTO_FORMAT_TRIAL) && !defined(MSWRITE_PATH_TRIAL)
                if (dirs_card_needs_qualification()) {
                    stage = dirs_card_qualify() ? CONFIRM : DONE;
                } else
#endif
                {
                    stage = DONE;
                    dirs_card_write(&staged_volume);
                }
            }
        }
#else
        else if ((pressed & PSP_CTRL_CROSS) && !ran) {
            ran = 1;
            heading();
            run_trial();
            pspDebugScreenPrintf("\nO: exit\n");
        }
#endif
        sceKernelDelayThread(20000);
    }
}
