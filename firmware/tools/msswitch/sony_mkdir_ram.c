/* GPL-3.0-or-later */
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <psputils.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <systemctrl.h>

#include "fat32_dirs.h"
#include "sony_mkdir_ram.h"

struct SonyRelocation {
    uint16_t offset;
    uint8_t kind, segment;
    int16_t low;
};

#include "sony_mkdir_profile.inc"

enum {
    SONY_MKDIR = 0x3814, SONY_MOUNT = 0xb7d4, SONY_UNMOUNT = 0xb910,
    SONY_READ = 0xaca8, SONY_WRITE = 0xb000, SONY_PREPARE = 0xba94,
    SONY_CREATE_MUTEX = 0xca3c, SONY_DELETE_MUTEX = 0xca74, SONY_MUTEX_ATTR = 0x301,
    CONTEXT_BYTES = 0x234, DIRECTORY_CACHE = 0x1fc,
    BLOCK_CACHE = 0x30, SECTOR_CACHE = 0x28, CACHE_STATE = 0xdc,
    MEDIA_STATE = 0x12c, MOUNT_COUNT = 0x130, BOUNCE = 0x13c,
    BLOCK_FD = 0x1d0, DEVICE = 0x1f0, BACKEND = 0x1f8,
    LABEL = 0x228, MUTEX = 0x230,
};

static uintptr_t sony_text;
static uint32_t profile_words[SONY_MKDIR_CODE_BYTES / 4];
static uint32_t context[CONTEXT_BYTES / 4];
static uintptr_t backend[13];
static unsigned char directory_cache[10][1668] __attribute__((aligned(64)));
static unsigned char block_cache[4][2048] __attribute__((aligned(64)));
static unsigned char sector_cache[3072] __attribute__((aligned(64)));
static unsigned char bounce[2 * FAT32_SECTOR_BYTES] __attribute__((aligned(64)));
static unsigned unexpected;

struct SonyDriverArg { const void *driver; uint32_t *context; };
struct SonyFileArg { uint32_t unused, unit; struct SonyDriverArg *driver; uint32_t flags; };
_Static_assert(offsetof(struct SonyFileArg, driver) == 8, "Sony file argument ABI");
_Static_assert(offsetof(struct SonyDriverArg, context) == 4, "Sony driver argument ABI");

static int kernel_range(uintptr_t address, unsigned size)
{
    return address >= 0x88000000 && address <= 0x8c000000 - size && !(address & 3);
}

static uint32_t normalize(uint32_t word, const struct SonyRelocation *rel, uint32_t base)
{
    switch (rel->kind) {
    case 2:
        return word - base;
    case 3:
    case 6:
    case 7:
        return (word & 0xfc000000) | ((word - (base >> 2)) & 0x03ffffff);
    case 4:
        base = (base + rel->low + 0x8000) >> 16;
        break;
    }
    return (word & 0xffff0000) | ((word - base) & 0xffff);
}

int sony_mkdir_ram_check(void)
{
    sony_text = 0;
    if (sceKernelDevkitVersion() != 0x06060110 || sceKernelGetModel() != 1)
        return 0;
    uintptr_t address = (uintptr_t)sceKernelFindModuleByName("sceMSFAT_Driver");
    if (!kernel_range(address, sizeof(SceModule2)))
        return 0;
    const SceModule2 *module = (const SceModule2 *)address;
    if (module->nsegment != 2 ||
        module->segmentaddr[0] != module->text_addr ||
        module->segmentsize[0] < SONY_MKDIR_CODE_BYTES ||
        !kernel_range(module->segmentaddr[0], SONY_MKDIR_CODE_BYTES) ||
        !kernel_range(module->segmentaddr[1], 4))
        return 0;
    memcpy(profile_words, (const void *)(uintptr_t)module->text_addr, sizeof(profile_words));
    for (unsigned i = 0; i < sizeof(sony_mkdir_relocations) / sizeof(sony_mkdir_relocations[0]); ++i) {
        const struct SonyRelocation *rel = &sony_mkdir_relocations[i];
        unsigned word = rel->offset / 4;
        profile_words[word] = normalize(profile_words[word], rel, module->segmentaddr[rel->segment]);
    }
    unsigned char digest[20];
    if (sceKernelUtilsSha1Digest((unsigned char *)profile_words, sizeof(profile_words), digest) ||
        memcmp(digest, sony_mkdir_digest, sizeof(digest)))
        return 0;
    sony_text = module->text_addr;
    return 1;
}

static int unsupported(void)
{
    ++unexpected;
    return (int)0x80010013;
}

static void prepare(const char *device, int fd, SceUID mutex)
{
    memset(context, 0, sizeof(context));
    memset(directory_cache, 0, sizeof(directory_cache));
    memset(block_cache, 0, sizeof(block_cache));
    memset(sector_cache, 0, sizeof(sector_cache));
    for (unsigned i = 0; i < sizeof(backend) / sizeof(backend[0]); ++i)
        backend[i] = (uintptr_t)unsupported;
    backend[7] = sony_text + SONY_READ;
    backend[8] = sony_text + SONY_WRITE;
    backend[10] = sony_text + SONY_PREPARE;
    for (unsigned i = 0; i < 10; ++i)
        context[DIRECTORY_CACHE / 4 + i] = (uintptr_t)directory_cache[i];
    for (unsigned i = 0; i < 4; ++i)
        context[(BLOCK_CACHE + i * 0x24) / 4] = (uintptr_t)block_cache[i];
    context[0x24 / 4] = UINT32_MAX;
    context[SECTOR_CACHE / 4] = (uintptr_t)sector_cache;
    context[0x2c / 4] = 6;
    context[0x120 / 4] = 2;
    context[CACHE_STATE / 4] = 0x2000;
    context[MEDIA_STATE / 4] = 0x0e;
    context[BOUNCE / 4] = (uintptr_t)bounce;
    context[BLOCK_FD / 4] = fd;
    context[DEVICE / 4] = (uintptr_t)device;
    context[BACKEND / 4] = (uintptr_t)backend;
    context[LABEL / 4] = (uintptr_t)device;
    context[MUTEX / 4] = mutex;
    unexpected = 0;
}

struct SonyMkdirResult sony_mkdir_ram_run(const char *device, int fd, unsigned sectors, int one_directory)
{
    struct SonyMkdirResult report = {.result = -1, .unmount = -1, .mutex_close = -1};
    if (!sony_text || fd < 0 || !sectors)
        return report;
    /* These are the bound ThreadManForKernel imports used by Sony's FAT init and exit. */
    SceUID (*create_mutex)(const char *, unsigned, int, const void *) =
        (void *)(sony_text + SONY_CREATE_MUTEX);
    int (*delete_mutex)(SceUID) = (void *)(sony_text + SONY_DELETE_MUTEX);
    SceUID mutex = create_mutex("PROX RAM FAT", SONY_MUTEX_ATTR, 0, NULL);
    if (mutex < 0) {
        report.result = mutex;
        return report;
    }
    prepare(device, fd, mutex);
    int (*mount)(uint32_t *, unsigned, const char *, unsigned) = (void *)(sony_text + SONY_MOUNT);
    int (*unmount)(uint32_t *) = (void *)(sony_text + SONY_UNMOUNT);
    int (*mkdir)(struct SonyFileArg *, const char *, int) = (void *)(sony_text + SONY_MKDIR);
    int (*write)(uint32_t *, const void *, uint32_t, unsigned) = (void *)(sony_text + SONY_WRITE);
    struct SonyDriverArg driver = {.context = context};
    struct SonyFileArg file = {.driver = &driver};
    report.result = mount(context, 0, device, 0);
    if (report.result == 0 && context[MOUNT_COUNT / 4] == 1) {
        unsigned limit = one_directory ? 1 : PSP_INITIAL_DIRECTORY_COUNT;
        for (; report.created < limit; ++report.created) {
            report.result = mkdir(&file, fat32_trial_directory(report.created), 0x0fff);
            if (report.result)
                break;
        }
        if (!one_directory && !report.result) {
            int at_end = write(context, bounce, sectors, 1);
            int across_end = write(context, bounce, sectors - 1, 2);
            report.boundary_rejected = at_end < 0 && across_end < 0;
        }
        report.unmount = unmount(context);
        if (!report.unmount && context[MOUNT_COUNT / 4])
            report.unmount = -1;
    }
    if (unexpected && !report.result)
        report.result = (int)0x80010013;
    report.mutex_close = delete_mutex(mutex);
    return report;
}
