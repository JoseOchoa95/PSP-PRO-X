/* GPL-3.0-or-later */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <string.h>
#include <systemctrl.h>

#include "probe.h"
#include "stock_reader.h"

PSP_MODULE_INFO("ProxMsAttributes", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

enum { CAPACITY_IOCTL = 0x02125006, SYSINFO_IOCTL = 0x02125803, SYSTEM_INFO = 0x10 };
static uint8_t page[SECTOR_BYTES] __attribute__((aligned(64)));
static uint8_t cached[MEDIA_ATTRIBUTE_BYTES] __attribute__((aligned(64)));
static SceUID raw = -1;

static int close_raw(void)
{
    if (raw < 0)
        return 1;
    if (sceIoClose(raw) < 0)
        return 0;
    raw = -1;
    return 1;
}

static int load_page(const struct StockReader *reader, uint32_t number)
{
    int result = -1;
    const char *error = stock_read_attribute_page(reader, number, page, &result);
    if (error || result != SECTOR_BYTES) {
        pspDebugScreenPrintf("Attribute page %u: 0x%08x\n", number, (unsigned)result);
        if (error)
            pspDebugScreenPrintf("%s\n", error);
        return 0;
    }
    return 1;
}

static int load_system_info(const struct StockReader *reader, uint32_t offset, uint8_t *output)
{
    unsigned skip = offset % SECTOR_BYTES;
    uint32_t number = offset / SECTOR_BYTES;
    unsigned first = SECTOR_BYTES - skip;
    if (first > MEDIA_ATTRIBUTE_BYTES)
        first = MEDIA_ATTRIBUTE_BYTES;
    if (!load_page(reader, number))
        return 0;
    memcpy(output, page + skip, first);
    if (first != MEDIA_ATTRIBUTE_BYTES) {
        if (!load_page(reader, number + 1))
            return 0;
        memcpy(output + first, page, MEDIA_ATTRIBUTE_BYTES - first);
    }
    return 1;
}

static void scan(void)
{
    uint32_t reported = 0;
    struct StockReader reader;
    raw = sceIoOpen("msstor:", PSP_O_RDONLY, 0);
    if (raw < 0 || sceIoIoctl(raw, CAPACITY_IOCTL, NULL, 0, &reported, sizeof(reported)) < 0 || !reported) {
        pspDebugScreenPrintf("Memory Stick capacity query failed.\n");
        return;
    }
    pspDebugScreenPrintf("Driver: %u sectors\n", reported);
    const char *error = stock_reader_init(&reader, reported);
    if (error) {
        pspDebugScreenPrintf("%s\n", error);
        return;
    }
    int status = sceIoIoctl(raw, SYSINFO_IOCTL, NULL, 0, cached, sizeof(cached));
    if (!load_page(&reader, 0))
        return;
    struct MsAttribute attributes[MS_ATTRIBUTE_MAX];
    unsigned count = decode_attribute_directory(page, attributes);
    if (!count) {
        pspDebugScreenPrintf("Memory Stick PRO attribute directory invalid.\n");
        return;
    }
    pspDebugScreenPrintf("Memory Stick PRO attributes: %u\n", count);
    for (unsigned i = 0; i < count; ++i) {
        const struct MsAttribute *attribute = &attributes[i];
        pspDebugScreenPrintf("ID %02x  offset %5u  size %4u\n",
                             (unsigned)attribute->id, attribute->offset, attribute->size);
    }
    for (unsigned i = 0; i < count; ++i) {
        const struct MsAttribute *attribute = &attributes[i];
        if (attribute->id != SYSTEM_INFO || attribute->size != sizeof(cached))
            continue;
        uint8_t info[MEDIA_ATTRIBUTE_BYTES];
        struct MediaGeometry geometry;
        if (load_system_info(&reader, attribute->offset, info) && decode_media_geometry(&geometry, info)) {
            pspDebugScreenPrintf("Raw System Info: %llu bytes\n", (unsigned long long)geometry.bytes);
            pspDebugScreenPrintf("Stock System Info: %s\n",
                                 status < 0 ? "unavailable" :
                                 memcmp(info, cached, sizeof(info)) ? "different" : "matches");
        }
        break;
    }
    pspDebugScreenPrintf("SD CSD pass-through: unverified\n");
}

static void heading(void)
{
    pspDebugScreenClear();
    pspDebugScreenPrintf("PRO-X Memory Stick Attribute Probe\n%s\n\n", MSDIAG_BUILD_ID);
}

int main(void)
{
    pspDebugScreenInit();
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    heading();
    pspDebugScreenPrintf("Wait for the Memory Stick light to go out.\n"
                         "Insert the card and adapter to inspect.\n\nX: scan    O: exit\n");
    unsigned previous = 0;
    for (;;) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if ((pressed & PSP_CTRL_CIRCLE) && close_raw() && sctrlKernelExitVSH(NULL) >= 0)
            return 0;
        if (pressed & PSP_CTRL_CROSS) {
            heading();
            if (close_raw())
                scan();
            if (!close_raw())
                pspDebugScreenPrintf("Could not close the read-only handle.\n");
            pspDebugScreenPrintf("\nX: rescan    O: exit\n");
        }
        sceKernelDelayThread(20000);
    }
}
