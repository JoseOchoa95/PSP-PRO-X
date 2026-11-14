/* PRO-X GitHub update dispatch, GPL-3.0-or-later.
 * Replaces the inherited PRO HTTP-list patch. No Sony instructions/offsets.
 * sceLibUpdateDL NIDs are documented by pspdev/psplibdoc libupdown.csv.
 * Physical method navigation remains to be tested. */
#ifdef PROX_UPDATE_NATIVE
#include "update_dispatch_shim.h"
#else
#include <pspkernel.h>
#include <pspsdk.h>
#include <psploadexec_kernel.h>
#include <pspiofilemgr.h>
#include <string.h>
#include "systemctrl.h"
#include "utils.h"
#include "libs.h"
#include "main.h"
#endif
#include <stdio.h>

#define UPDATE_ERROR ((int)0x80010002)
static volatile int launching;

static void dispatch_log(const char *event, int result)
{
    char line[128];
    int fd, n;
    fd = sceIoOpen("ms0:/PSP/GAME/PROXUPDATE/dispatch.txt", PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0666);
    if (fd < 0) fd = sceIoOpen("ef0:/PSP/GAME/PROXUPDATE/dispatch.txt", PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0666);
    if (fd < 0) return;
    /* event is one of the fixed literals below, all shorter than 48 bytes. */
    n = sprintf(line, "%s result=0x%08x fw=0x%08x\n", event, result, psp_fw_version);
    sceIoWrite(fd, line, n); sceIoClose(fd);
}

int proxInternetUpdate(void)
{
    const char *paths[] = {"ms0:/PSP/GAME/PROXUPDATE/EBOOT.PBP",
                           "ef0:/PSP/GAME/PROXUPDATE/EBOOT.PBP"};
    struct SceKernelLoadExecVSHParam param;
    int k1 = pspSdkSetK1(0), i, rc = UPDATE_ERROR;
    if (!conf.useownupdate) {
        dispatch_log("disabled (no network fallback)", UPDATE_ERROR);
        pspSdkSetK1(k1); return UPDATE_ERROR;
    }
    if (launching) { pspSdkSetK1(k1); return UPDATE_ERROR; }
    launching = 1;
    dispatch_log("Internet import reached", 0);
    for (i = 0; i < 2; ++i) {
        SceIoStat stat;
        if (sceIoGetstat(paths[i], &stat) < 0 || stat.st_size < 40) continue;
        memset(&param, 0, sizeof(param));
        param.size = sizeof(param);
        param.args = strlen(paths[i]) + 1;
        param.argp = (void *)paths[i];
        param.key = "game";
        rc = i ? sctrlKernelLoadExecVSHEf2(paths[i], &param) :
                 sctrlKernelLoadExecVSHMs2(paths[i], &param);
        break;
    }
    launching = 0;
    dispatch_log("client launch failed", rc);
    pspSdkSetK1(k1);
    return rc;
}

static int in_module(SceModule2 *mod, unsigned int address, unsigned int size)
{
    unsigned int i;
    if (!size || address + size < address || mod->nsegment > 4) return 0;
    for (i = 0; i < mod->nsegment; ++i)
        if (address >= mod->segmentaddr[i] && size <= mod->segmentsize[i] &&
            address - mod->segmentaddr[i] <= mod->segmentsize[i] - size) return 1;
    return 0;
}

/* Validate tables and collect destinations before any writes. */
static int hook_download_imports(SceModule2 *mod)
{
    unsigned int offset = 0, sites[16], count = 0, syscall;
    if (((unsigned int)mod->stub_top & 3) ||
        !in_module(mod, (unsigned int)mod->stub_top, mod->stub_size) ||
        mod->stub_size > 16384) return -1;
    while (offset < mod->stub_size) {
        PspModuleImport *table = (void *)((unsigned int)mod->stub_top + offset);
        unsigned int bytes, j;
        if (mod->stub_size - offset < 20) return -1;
        bytes = table->entLen * 4;
        if (bytes < 20 || bytes > mod->stub_size - offset) return -1;
        if (table->name && in_module(mod, (unsigned int)table->name, 15) &&
            !memcmp(table->name, "sceLibUpdateDL", 15)) {
            if (((unsigned int)table->fnids & 3) || ((unsigned int)table->funcs & 3) ||
                table->funcCount > 128 ||
                !in_module(mod, (unsigned int)table->fnids, table->funcCount * 4) ||
                !in_module(mod, (unsigned int)table->funcs, table->funcCount * 8)) return -1;
            for (j = 0; j < table->funcCount; ++j) {
                unsigned int nid = table->fnids[j];
                /* Init/InitEx dispatch before the old engine starts.
                 * ConnectServer also blocked against fallthrough. */
                if (nid == 0xFC1AB540 || nid == 0xF6690A9A || nid == 0xFD675E8D) {
                    if (count == 16) return -1;
                    sites[count++] = (unsigned int)&table->funcs[j * 2];
                }
            }
        }
        offset += bytes;
    }
    if (!count) return -1;
    syscall = sctrlKernelQuerySystemCall(proxInternetUpdate);
    if (syscall == (unsigned int)-1) return -1;
    for (offset = 0; offset < count; ++offset) {
        _sw(0x03e00008, sites[offset]);
        _sw(MAKE_SYSCALL(syscall), sites[offset] + 4);
    }
    return 0;
}

static void stop_unknown_module(SceModule2 *mod)
{
    /* Declared entry point, not an offset/signature guess. Refuse startup on
     * unknown layouts rather than executing the original network updater. */
    unsigned int entry = mod->entry_addr;
    if (!(entry & 3) && entry >= mod->text_addr && mod->text_size >= 8 &&
        entry - mod->text_addr <= mod->text_size - 8) {
        _sw(0x03e00008, entry); /* jr ra */
        _sw(0x3c028001, entry + 4); /* error 0x80010000 */
    }
}

void patch_update_plugin_module(SceModule *mod)
{
    SceModule2 *module = (SceModule2 *)mod;
    int result = psp_fw_version == FW_661 ? hook_download_imports(module) : -1;
    dispatch_log("update_plugin imports", result);
    if (result < 0)
        stop_unknown_module(module);
}

void patch_SceUpdateDL_Library(u32 unused)
{
    SceModule *plugin = sceKernelFindModuleByName("update_plugin_module");
    (void)unused;
    /* Reapply after dynamic linking of libupdown, before its entry point. */
    if (plugin) patch_update_plugin_module(plugin);
}
