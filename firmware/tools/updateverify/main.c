/* Read-only PSP diagnostic for the dedicated EBOOT parser. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <stdio.h>
#include <stdlib.h>
#include "kubridge.h"
#include "dedicated_update.h"
#include "expected_update.h"

PSP_MODULE_INFO("ProxUpdateVerify", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(4096);

static int verify(ProxDedicatedExpected *expected, ProxDedicatedUpdate *update)
{
    const char *path = "ms0:/PSP/GAME/PROXEBOOT/UPDATE.PBP";
    unsigned char *data = malloc(expected->size);
    if (!data)
        return -1;
    int status = -1;
    FILE *file = fopen(path, "rb");
    if (file) {
        if (fread(data, 1, expected->size, file) == expected->size && fgetc(file) == EOF && !ferror(file))
            status = prox_dedicated_update(data, expected->size, expected, update);
        if (fclose(file))
            status = -1;
    }
    free(data);
    return status;
}

int main(void)
{
    ProxDedicatedExpected expected = fixture;
    ProxDedicatedUpdate update;
    SceCtrlData pad;
    pspDebugScreenInit();
    expected.model = kuKernelGetModel();
    expected.devkit = sceKernelDevkitVersion();
    pspDebugScreenPrintf("PRO-X %s dedicated update check\n", expected.build_id);
    if (!verify(&expected, &update)) {
        pspDebugScreenPrintf("PASS: dedicated EBOOT and %u parts verified.\n", PROX_DEDICATED_PARTS);
        pspDebugScreenPrintf("Verified %u bytes. Flash was untouched.\n", expected.size);
    } else {
        pspDebugScreenPrintf("FAIL: check model, firmware and matching UPDATE.PBP.\n");
    }
    pspDebugScreenPrintf("Press X to return to XMB.\n");
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    while (sceCtrlReadBufferPositive(&pad, 1) > 0 && !(pad.Buttons & PSP_CTRL_CROSS)) {
    }
    sceKernelExitGame();
    return 0;
}
