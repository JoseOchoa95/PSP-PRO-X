/* Read-only use of three verified Sony 6.61 FAT block callbacks. GPL-3.0-or-later. */
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <psputils.h>
#include <stddef.h>
#include <string.h>
#include <systemctrl.h>

#include "native_fat_read.h"
#include "fat32_ram.h"
#ifdef MSFORMAT_TRIAL
#include "stock_reader.h"
#endif

enum {
    FAT_TEXT_BYTES = 0xcff8,
    UPDATE_STATE = 0x5bb4,
    UPDATE_STATE_END = 0x5c18,
    FAT_DEVCTL = 0x6c18,
    FAT_FORMAT = 0x6e80,
    FAT_FORMAT_END = 0x6f4c,
    FAT_READ = 0xaca8,
    FAT_READ_END = 0xb000,
    FAT_MOUNT = 0xb7d4,
    FAT_UNMOUNT = 0xb910,
    FAT_UNMOUNT_END = 0xb974,
    MEDIUM_OPEN = 0x02,
    MEDIUM_READY = 0x08,
    READ_ONLY = 0x20,
};

static const uint32_t format_command = 0x0241d813;
static const int invalid_profile = (int)0x80010016;
static const int format_io_error = (int)0x80010005;

_Static_assert(offsetof(struct NativeFatContext, state) == 0x12c, "FAT state");
_Static_assert(offsetof(struct NativeFatContext, mounts) == 0x130, "FAT mount count");
_Static_assert(offsetof(struct NativeFatContext, bounce) == 0x13c, "FAT bounce buffer");
_Static_assert(offsetof(struct NativeFatContext, fd) == 0x1d0, "FAT partition fd");
_Static_assert(offsetof(struct NativeFatContext, device) == 0x1f0, "FAT device name");

typedef int (*StockMount)(struct NativeFatContext *, unsigned, const char *, unsigned);
typedef int (*StockRead)(struct NativeFatContext *, void *, uint32_t, unsigned);
typedef int (*StockUnmount)(struct NativeFatContext *);

static int kernel_range(uintptr_t address, unsigned size)
{
    return address >= 0x88000000 && address <= 0x8c000000 - size && (address & 3) == 0;
}

static int matches_code(uintptr_t text, unsigned start, unsigned end, const char *expected)
{
    uint32_t normalized[(FAT_READ_END - FAT_READ) / sizeof(uint32_t)];
    unsigned char digest[DIGEST_BYTES];
    unsigned count = (end - start) / sizeof(uint32_t);
    if (count > sizeof(normalized) / sizeof(normalized[0]))
        return 0;
    for (unsigned i = 0; i < count; ++i) {
        uintptr_t pc = text + start + i * sizeof(uint32_t);
        uint32_t word = *(const volatile uint32_t *)pc;
        unsigned opcode = word >> 26;
        if (opcode == 2 || opcode == 3) {
            uint32_t target = ((pc + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2);
            if (target < text || target - text >= FAT_TEXT_BYTES)
                return 0;
            word = (word & 0xfc000000) | ((target - text) >> 2);
        }
        normalized[i] = word;
    }
    return !sceKernelUtilsSha1Digest((unsigned char *)normalized, end - start, digest) &&
           !memcmp(digest, expected, sizeof(digest));
}

const char *native_fat_check(struct NativeFatRead *reader)
{
    reader->text = 0;
    if (sceKernelDevkitVersion() != 0x06060110 || sceKernelGetModel() != 1)
        return "Sony FAT trial requires PSP model ID 1 on 6.61.";
    uintptr_t address = (uintptr_t)sceKernelFindModuleByName("sceMSFAT_Driver");
    if (!kernel_range(address, sizeof(SceModule2)))
        return "Sony FAT module is unavailable.";
    const SceModule2 *module = (const SceModule2 *)address;
    uintptr_t text = module->segmentaddr[0];
    if (module->nsegment != 2 || text != module->text_addr ||
        module->segmentsize[0] < FAT_TEXT_BYTES || !kernel_range(text, FAT_TEXT_BYTES))
        return "Sony FAT module layout is unsupported.";

    /* The covered regions have only same-text J and JAL relocations. */
    if (!matches_code(text, UPDATE_STATE, UPDATE_STATE_END,
                      "\xb1\xe3\x33\x16\x55\x2b\x0e\x3a\xb5\x98\x35\x38\x05\x5d\x40\xdb\xab\x71\x08\x6d") ||
        !matches_code(text, FAT_READ, FAT_READ_END,
                      "\xa7\x3e\xd4\xdd\x1e\x0a\x20\x05\xd9\xab\x28\xdb\x1d\x77\x91\x41\x84\xcb\x9a\xc6") ||
        !matches_code(text, FAT_MOUNT, FAT_UNMOUNT_END,
                      "\x55\xb1\x55\x32\x09\xb7\x34\xc1\xac\x46\x74\xfc\x00\x34\x80\x28\xf3\xd1\x4f\xf2"))
        return "Sony FAT callbacks did not match the verified profile.";

    reader->text = text;
    return NULL;
}

int native_fat_mount(struct NativeFatRead *reader, const char *device, int fd)
{
    if (!reader->text || reader->context.mounts || fd < 0)
        return -1;
    /* The bounded read-only fd is open. Skip stock detection and device selection. */
    reader->context = (struct NativeFatContext){
        .state = MEDIUM_OPEN | MEDIUM_READY | READ_ONLY,
        .bounce = reader->sector,
        .fd = fd,
        .device = device,
    };
    StockMount mount = (StockMount)(reader->text + FAT_MOUNT);
    return mount(&reader->context, 0, device, IOASSIGN_RDONLY);
}

int native_fat_read(struct NativeFatRead *reader, uint32_t lba)
{
    if (!reader->text || reader->context.mounts != 1)
        return -1;
    StockRead read = (StockRead)(reader->text + FAT_READ);
    return read(&reader->context, reader->sector, lba, 1);
}

int native_fat_unmount(struct NativeFatRead *reader)
{
    if (!reader->text || reader->context.mounts != 1)
        return -1;
    StockUnmount unmount = (StockUnmount)(reader->text + FAT_UNMOUNT);
    int result = unmount(&reader->context);
    memset(&reader->context, 0, sizeof(reader->context));
    return result;
}

struct FormatSession {
    struct NativeFatContext context;
    int (*apply)(void *);
    int (*verify)(void *);
    void *target;
};

struct FormatCallbacks {
    void *unused_before_refresh[6];
    int (*refresh)(struct NativeFatContext *);
    void *unused_before_format[2];
    int (*format)(struct NativeFatContext *, int);
};

struct NativeFormatRequest {
    struct NativeFatContext *context;
    const char *device;
    uint32_t command;
    const uint32_t *const *arguments;
    uint32_t argument_bytes;
    void *output;
    uint32_t output_bytes;
};

_Static_assert(offsetof(struct FormatCallbacks, refresh) == 0x18, "FAT refresh callback");
_Static_assert(offsetof(struct FormatCallbacks, format) == 0x24, "FAT format callback");
_Static_assert(offsetof(struct NativeFormatRequest, argument_bytes) == 0x10, "FAT command packet");
_Static_assert(sizeof(struct NativeFormatRequest) == 0x1c, "FAT packet size");

static int refresh_format(struct NativeFatContext *context)
{
    struct FormatSession *session = (struct FormatSession *)context;
    return session->verify(session->target) ? 0 : format_io_error;
}

static int format_selected(struct NativeFatContext *context, int mode)
{
    struct FormatSession *session = (struct FormatSession *)context;
    if (mode != 0)
        return invalid_profile;
    return session->apply(session->target) ? 0 : format_io_error;
}

static int dispatch_format(const struct NativeFatRead *reader, int (*apply)(void *),
                           int (*verify)(void *), void *target)
{
    if (!reader->text || reader->context.mounts != 1 ||
        !matches_code(reader->text, FAT_DEVCTL, FAT_FORMAT,
                      "\xd2\x17\x6a\x44\xf3\xe2\xe1\x76\x3d\x18\x31\x4d\x18\x98\xda\x76\x71\x3d\x6a\x79") ||
        !matches_code(reader->text, FAT_FORMAT, FAT_FORMAT_END,
                      "\x11\x15\x2c\x1b\x14\x9b\xe7\x60\x25\x41\xcf\xfc\xda\xaf\xd8\x00\x9b\x5f\xc1\xd2"))
        return invalid_profile;

    struct FormatCallbacks callbacks = {.refresh = refresh_format, .format = format_selected};
    /* A private callback table replaces both the stock format and media refresh. */
    struct FormatSession session = {
        .context = {.fd = -1, .backend = &callbacks},
        .apply = apply, .verify = verify, .target = target,
    };
    const uint32_t mode = 0;
    const uint32_t *const arguments = &mode;
    struct NativeFormatRequest packet = {
        .context = &session.context,
        .command = format_command,
        .arguments = &arguments,
        .argument_bytes = sizeof(arguments),
    };
    int (*dispatch)(struct NativeFormatRequest *) =
        (int (*)(struct NativeFormatRequest *))(reader->text + FAT_DEVCTL);
    return dispatch(&packet);
}

static int initialize_ram(void *target)
{
    struct Fat32Ram *volume = target;
    return !volume->writes && !volume->used &&
           fat32_initialize(volume->start, volume->length, 0x50585031,
                            fat32_ram_write, volume) == FAT32_INIT_OK;
}

static int verify_ram(void *target)
{
    return fat32_ram_verify(target);
}

int native_fat_format_ram(const struct NativeFatRead *reader, struct Fat32Ram *volume)
{
    return dispatch_format(reader, initialize_ram, verify_ram, volume);
}

#ifdef MSFORMAT_TRIAL
static int initialize_card(void *target)
{
    return stock_format_partition_trial(target);
}

static int verify_card(void *target)
{
    const struct StockFormatTrial *trial = target;
    return !trial->error && trial->worker_result == SECTOR_BYTES && trial->writes > 0 &&
           trial->writes == trial->verified + 2;
}

int native_fat_format_card(const struct NativeFatRead *reader, struct StockFormatTrial *trial)
{
    return dispatch_format(reader, initialize_card, verify_card, trial);
}
#endif
