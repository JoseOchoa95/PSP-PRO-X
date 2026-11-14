/* PRO-X system-information product label, GPL-3.0-or-later.
 * A separately allocated user-readable UTF-16 buffer avoids assuming that
 * Sony's original string storage can hold a longer product version. */
#ifdef PROX_VERSION_NATIVE
#include "version_display_shim.h"
#else
#include <pspkernel.h>
#include <pspsdk.h>
#include "systemctrl.h"
#endif
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "prox_product.h"
#include "version_display.h"

static const char product_label[] = "6.61 PRO-X " PROX_PRODUCT_VERSION;
_Static_assert(sizeof(product_label) <= 64, "System version label is too long");
_Static_assert(sizeof(unsigned short) == 2, "PSP UTF-16 unit size mismatch");

static unsigned short *user_label;
static int label_block = -1;
static struct {
    unsigned int before[2], services;
    int allocation, anchors;
} diagnostic;

static int finish(void *text, size_t text_size, size_t offset, int status)
{
#ifndef PROX_VERSION_NATIVE
    const char *paths[] = {"ms0:/PSP/GAME/PROXUPDATE/display.txt",
                           "ef0:/PSP/GAME/PROXUPDATE/display.txt"};
    char line[384];
    unsigned int i;
    int n;
    (void)text;
    n = sprintf(line,
        "PRO-X display rev=2 product=%s status=%d\n"
        "offset=%08x text_size=%08x before=%08x,%08x anchors=%d services=%x allocation=%08x label=%08x bytes=%u\n",
        PROX_PRODUCT_VERSION, status, (unsigned int)offset, (unsigned int)text_size,
        diagnostic.before[0], diagnostic.before[1], diagnostic.anchors,
        diagnostic.services, diagnostic.allocation, (unsigned int)(uintptr_t)user_label,
        (unsigned int)(sizeof(product_label)*2));
    for (i = 0; i < 2; ++i) {
        SceIoStat stat;
        int fd;
        if (sceIoGetstat(paths[i], &stat) >= 0 &&
            (stat.st_size < 0 || stat.st_size > 32768-(int)sizeof(line))) continue;
        fd = sceIoOpen(paths[i], PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0666);
        if (fd < 0) continue;
        sceIoWrite(fd, line, n); sceIoClose(fd); break;
    }
#else
    (void)text; (void)text_size; (void)offset;
#endif
    return status;
}

static unsigned short *prepare_label(void)
{
    typedef int (*Alloc)(int, const char *, int, unsigned int, void *);
    typedef void *(*Head)(int);
    typedef int (*Free)(int);
    Alloc allocate;
    Head head;
    Free release;
    unsigned short *buffer;
    unsigned int size = sizeof(product_label) * sizeof(*buffer);
    size_t i;
    int block, k1;
    if (user_label) {
        diagnostic.services = 7; diagnostic.allocation = label_block;
        return user_label;
    }
    /* This is a kernel VSH module. Use the kernel exports and the existing
     * SystemControl 6.61 NID resolver, as recorded in nid_660_data.c/CrossFW. */
    allocate = (Alloc)(uintptr_t)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0x237DBD4F);
    head = (Head)(uintptr_t)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0x9D9A5BA1);
    release = (Free)(uintptr_t)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0xB6D61D02);
    diagnostic.services = (allocate ? 1u : 0u) | (head ? 2u : 0u) | (release ? 4u : 0u);
    if (!allocate || !head || !release) return NULL;
    k1 = pspSdkSetK1(0);
    block = allocate(2, "PRO-X system version", PSP_SMEM_High, size, NULL);
    diagnostic.allocation = block;
    if (block < 0) { pspSdkSetK1(k1); return NULL; }
    buffer = head(block);
    /* A kernel pointer cannot be passed to the user-mode sysconf plugin. */
    if ((uintptr_t)buffer < 0x08800000 || (uintptr_t)buffer > 0x0c000000u-size ||
        ((uintptr_t)buffer & 1)) {
        release(block); pspSdkSetK1(k1); return NULL;
    }
    for (i = 0; i < sizeof(product_label); ++i) buffer[i] = (unsigned char)product_label[i];
    /* Keep one block for the lifetime of VSH; repeated sysconf loads reuse it.
     * The VSH user partition is reclaimed by loadexec/reboot. Never free a
     * buffer while a sysconf consumer can still hold its pointer. */
    user_label = buffer;
    label_block = block;
    pspSdkSetK1(k1);
    return user_label;
}

int prox_system_version_patch(void *text, size_t text_size, size_t offset)
{
    unsigned int *instructions;
    unsigned short *label;
    uintptr_t address = (uintptr_t)text;
    size_t i, found = 0;
    memset(&diagnostic, 0, sizeof(diagnostic)); diagnostic.allocation = -1;
    if (!text || (address & 3) || (offset & 3) || text_size < 28 ||
        text_size > 1024*1024 || offset > text_size-8 ||
        address > UINTPTR_MAX-text_size) return finish(text,text_size,offset,-1);
    instructions = (void *)(address+offset);
    diagnostic.before[0] = instructions[0]; diagnostic.before[1] = instructions[1];
    /* Upstream ARK identifies this hook with 0x34c600c9 and a NOP at +8,
     * then writes the pair at +20. Pin/provenance: updater-dependencies.json.
     * Require one anchor agreeing with the inherited 6.61 offset. Do not
     * assume that Sony's original pair is already our LUI/ORI sequence. */
    for (i = 0; i <= text_size-28; i += 4) {
        const unsigned int *candidate = (void *)(address+i);
        if (candidate[0] == 0x34c600c9u && candidate[2] == 0) {
            ++diagnostic.anchors; found = i+20;
        }
    }
    if (diagnostic.anchors != 1 || found != offset) return finish(text,text_size,offset,-2);
    label = prepare_label();
    if (!label) return finish(text,text_size,offset,-3);
    address = (uintptr_t)label;
    instructions[0] = 0x3c020000u | ((unsigned int)(address >> 16) & 0xffffu);
    instructions[1] = 0x34420000u | ((unsigned int)address & 0xffffu);
    return finish(text,text_size,offset,0);
}
