/* Read-only diagnostic for the verified Sony 6.61 sceMSstor_Driver layout. GPL-3.0-or-later. */
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <psputils.h>
#include <stddef.h>
#include <string.h>
#include <systemctrl.h>

#include "probe.h"
#include "stock_reader.h"
#ifdef MSMULTI_CARD_TRIAL
#include "capacity.h"
#endif
#ifdef MSDIAG_WRITE_TRIAL
#include "write_trial.h"
#endif
#ifdef MSFORMAT_TRIAL
#include "fat32_init.h"
#endif
#ifdef MSDIRS_CARD_TRIAL
#include "fat32_dirs.h"
#endif

enum {
    TEXT_BYTES = 0xf2f4,
    DATA_BYTES = 0x584c,
    STOCK_READ = 0x7a4,
    STOCK_READ_END = 0xa44,
    RAW_DISPATCH = 0x3228,
    RAW_DISPATCH_END = 0x3268,
    PRO_READ = 0x795c,
    PRO_READ_END = 0x7b78,
    PRO_TABLE = 0xeb3c,
    IO_CONTEXT = 0x20,
    ACTIVE_TABLE = 0x164,
    MEDIA_CONTEXT = 0x200,
    ACTIVE_MEDIA = 0x5094,
    CONTROLLER = 0x4c00,
    MEDIA_TYPE = 0x4c14,
    SECTOR_COUNT = 0x4e60,
};

/* The SDK exposes the prefix only. Sony's read worker uses arg and byte offset. */
struct StockFile {
    PspIoDrvFileArg file;
    uint32_t private_word;
    uint64_t position;
};

typedef int (*ReadDispatch)(uint32_t lba, unsigned count, void *buffer);
typedef int (*StockRead)(struct StockFile *file, void *buffer, unsigned bytes,
                         unsigned mode, ReadDispatch dispatch);

struct TracedRead {
    ReadDispatch dispatch;
    struct StockReadTrace *trace;
    uint32_t lba;
};

/* MSDIAG reads on its main thread. The stock worker calls this synchronously
   while it owns the I/O mutex. */
static struct TracedRead *active_read;

static int trace_read(uint32_t lba, unsigned count, void *buffer)
{
    struct TracedRead *read = active_read;
    int result;
    if (!read || lba != read->lba || count != 1)
        return -1;
    result = read->dispatch(lba, count, buffer);
    if (!read->trace->calls)
        read->trace->first_result = result;
    read->trace->last_result = result;
    read->trace->calls++;
    return result;
}

_Static_assert(offsetof(struct StockFile, file.arg) == 0x10, "stock device argument");
_Static_assert(offsetof(struct StockFile, position) == 0x18, "stock byte position");
_Static_assert(MEDIA_CONTEXT + SECTOR_COUNT + 4 <= DATA_BYTES, "stock media bounds");

struct AddressRelocation {
    uint16_t hi_offset, lo_offset;
    uint32_t hi_word, lo_word;
    int data_segment;
};

static const struct AddressRelocation address_relocations[] = {
    {0x086c, 0x0870, 0x3c060001, 0x24c6eac0, 0},
    {0x09a4, 0x09a8, 0x3c060001, 0x24c6ead4, 0},
    {0x3230, 0x3238, 0x3c020000, 0x8c420164, 1},
    {0x3234, 0x3240, 0x3c030000, 0x8c645094, 1},
    {0x79a4, 0x79a8, 0x3c060001, 0x24c6ecc8, 0},
#ifdef MSDIAG_ATTRIBUTE_PROBE
    {0x7780, 0x7784, 0x3c060001, 0x24c6ec4c, 0},
#endif
#ifdef MSDIAG_WRITE_TRIAL
    {0x32d4, 0x32dc, 0x3c020000, 0x8c435094, 1},
    {0x330c, 0x3310, 0x3c020000, 0x8c420164, 1},
    {0x7bac, 0x7bb0, 0x3c060001, 0x24c6ed44, 0},
#endif
};

static uint32_t runtime_word(uintptr_t address)
{
    return *(const volatile uint32_t *)address;
}

/* Restore the original instruction encoding only after checking both relocated words. */
static int normalize_addresses(const struct StockReader *reader, unsigned start,
                               unsigned end, uint32_t *code)
{
    size_t i;
    for (i = 0; i < sizeof(address_relocations) / sizeof(address_relocations[0]); ++i) {
        const struct AddressRelocation *reloc = &address_relocations[i];
        uint32_t address, hi, lo;
        unsigned hi_index, lo_index;
        if (reloc->hi_offset < start || reloc->lo_offset >= end)
            continue;
        address = reloc->data_segment ? reader->data : reader->text;
        address += (reloc->hi_word << 16) + (int16_t)reloc->lo_word;
        hi = (reloc->hi_word & 0xffff0000) | ((address + 0x8000) >> 16);
        lo = (reloc->lo_word & 0xffff0000) | (address & 0xffff);
        hi_index = (reloc->hi_offset - start) / 4;
        lo_index = (reloc->lo_offset - start) / 4;
        if (code[hi_index] != hi || code[lo_index] != lo)
            return 0;
        code[hi_index] = reloc->hi_word;
        code[lo_index] = reloc->lo_word;
    }
    return 1;
}

static int matches_code(const struct StockReader *reader, unsigned start,
                        unsigned end, const char *expected_digest)
{
    uint32_t code[(STOCK_READ_END - STOCK_READ) / 4];
    unsigned i, count = (end - start) / 4;
    unsigned char digest[DIGEST_BYTES];
    if (count > sizeof(code) / sizeof(code[0]))
        return 0;
    for (i = 0; i < count; ++i) {
        uint32_t pc = reader->text + start + i * 4;
        uint32_t word = runtime_word(pc);
        if (word >> 26 == 2 || word >> 26 == 3) {
            uint32_t target = ((pc + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2);
            if (target < reader->text || target - reader->text >= TEXT_BYTES)
                return 0;
            word = (word & 0xfc000000) | ((target - reader->text) >> 2);
        }
        code[i] = word;
    }
    return normalize_addresses(reader, start, end, code) &&
           sceKernelUtilsSha1Digest((unsigned char *)code, end - start, digest) == 0 &&
           !memcmp(digest, expected_digest, sizeof(digest));
}

static int kernel_range(uintptr_t address, unsigned size)
{
    return address >= 0x88000000 && address <= 0x8c000000 - size &&
           (address & 3) == 0;
}

const char *stock_reader_init(struct StockReader *reader, uint32_t reported_blocks)
{
    uintptr_t module_address;
    const SceModule2 *module;
    if (sceKernelDevkitVersion() != 0x06060110 || sceKernelGetModel() != 1)
        return "Target requires PSP model ID 1 on 6.61.";
    module_address = (uintptr_t)sceKernelFindModuleByName("sceMSstor_Driver");
    if (!kernel_range(module_address, sizeof(*module)))
        return "Stock Memory Stick module is unavailable.";
    module = (const SceModule2 *)module_address;
    if (module->nsegment != 2 || module->segmentaddr[0] != module->text_addr ||
        module->segmentsize[0] < TEXT_BYTES || module->segmentsize[1] < DATA_BYTES ||
        !kernel_range(module->segmentaddr[0], TEXT_BYTES) ||
        !kernel_range(module->segmentaddr[1], DATA_BYTES) ||
        module->segmentaddr[1] < module->segmentaddr[0] + TEXT_BYTES)
        return "Stock Memory Stick module layout unavailable.";
    reader->text = module->segmentaddr[0];
    reader->data = module->segmentaddr[1];
    reader->reported_blocks = reported_blocks;
    /* SHA-1 of the three original code ranges before the PSP loader relocates them. */
    if (!matches_code(reader, STOCK_READ, STOCK_READ_END,
                      "\xf0\x0c\x77\xd9\x79\xa9\xfc\x4a\x29\x4d\x6b\x5b\xaf\xf1\x52\xca\x3c\x35\x8c\x84") ||
        !matches_code(reader, RAW_DISPATCH, RAW_DISPATCH_END,
                      "\x18\xae\xfd\x62\xc4\x5e\x4f\x2f\xc0\x89\xf3\x45\xfc\x15\x03\x28\xb7\xde\xa6\xef") ||
        !matches_code(reader, PRO_READ, PRO_READ_END,
                      "\xaf\x55\x2f\x1a\x50\x40\x74\x2d\xf4\xe0\x7f\x4a\xd3\x91\x0a\xf6\x34\xc5\x5e\x6f"))
        return "Stock read code did not match the 6.61 profile.";
    return NULL;
}

static const char *prepare_read(const struct StockReader *reader, uint32_t lba,
                                struct StockFile *request)
{
    uintptr_t io = reader->data + IO_CONTEXT;
    uintptr_t media = reader->data + MEDIA_CONTEXT;
    uint32_t type = runtime_word(media + MEDIA_TYPE);
    if ((type != 64 && type != 65) ||
        runtime_word(reader->data + ACTIVE_TABLE) != reader->text + PRO_TABLE ||
        runtime_word(reader->text + PRO_TABLE) != reader->text + PRO_READ)
        return "Active Memory Stick PRO read path differs.";
    if ((int32_t)runtime_word(io) <= 0 || !runtime_word(io + 0x10) ||
        runtime_word(io + 8) != media || runtime_word(reader->data + ACTIVE_MEDIA) != media ||
        !runtime_word(io + 4) || runtime_word(io + 4) != runtime_word(media + CONTROLLER) ||
        runtime_word(media + SECTOR_COUNT) != reader->reported_blocks)
        return "Stock I/O context or media changed.";
    request->file.arg = (void *)io;
    request->position = (uint64_t)lba * SECTOR_BYTES;
    return NULL;
}

const char *stock_read_sector(const struct StockReader *reader, uint32_t lba,
                              void *buffer, int *result, struct StockReadTrace *trace)
{
    struct StockFile request = {0};
    const char *error = prepare_read(reader, lba, &request);
    if (error)
        return error;
    StockRead read = (StockRead)(reader->text + STOCK_READ);
    ReadDispatch dispatch = (ReadDispatch)(reader->text + RAW_DISPATCH);
    struct TracedRead traced = {dispatch, trace, lba};
    /* Same mode and dispatch as Sony's whole-device read wrapper. The worker
       handles its own mutex, power and completion. The position is local. */
    if (trace) {
        *trace = (struct StockReadTrace){0};
        active_read = &traced;
        dispatch = trace_read;
    }
    *result = read(&request, buffer, SECTOR_BYTES, 0, dispatch);
    active_read = NULL;
    return NULL;
}

#ifdef MSDIAG_ATTRIBUTE_PROBE
enum { PRO_READ_ATTRIBUTES = 0x7738, ATTRIBUTE_PAGES = 64 };
typedef int (*ReadAttributes)(void *media, uint32_t page, int count, void *buffer);

static const struct StockReader *attribute_reader;
static uint32_t attribute_page;
static unsigned attribute_calls;

static int read_attributes(uint32_t lba, unsigned count, void *buffer)
{
    const struct StockReader *reader = attribute_reader;
    struct StockFile request = {0};
    if (!reader || lba || count != 1 || attribute_calls++ || prepare_read(reader, 0, &request))
        return -1;
    return ((ReadAttributes)(reader->text + PRO_READ_ATTRIBUTES))(
        (void *)(reader->data + MEDIA_CONTEXT), attribute_page, 1, buffer);
}

const char *stock_read_attribute_page(const struct StockReader *reader, uint32_t page, void *buffer,
                                     int *result)
{
    struct StockFile request = {0};
    const char *error = prepare_read(reader, 0, &request);
    if (error)
        return error;
    if (page >= ATTRIBUTE_PAGES || attribute_reader || ((uintptr_t)buffer & 63))
        return "Invalid attribute page or transfer buffer.";
    if (!matches_code(reader, PRO_READ_ATTRIBUTES, PRO_READ,
                      "\x80\x50\xbd\xfd\xf0\x34\x45\x0c\x4e\xfc\x81\xb4\x8e\x9c\x06\xc1\xc1\x5c\x8f\x2d"))
        return "Stock attribute read code did not match the 6.61 profile.";
    attribute_reader = reader;
    attribute_page = page;
    attribute_calls = 0;
    *result = ((StockRead)(reader->text + STOCK_READ))(&request, buffer, SECTOR_BYTES, 0, read_attributes);
    attribute_reader = NULL;
    return attribute_calls == 1 ? NULL : "The stock worker did not complete one attribute read.";
}
#endif

#ifdef MSDIAG_WRITE_TRIAL
enum {
    RAW_WRITE = 0x32cc,
    RAW_WRITE_END = 0x3334,
    PRO_WRITE = 0x7b64,
    PRO_WRITE_END = 0x7d78,
};

#ifndef MSFORMAT_TRIAL
struct WriteTrial {
    const struct StockReader *reader;
    const void *original;
    const void *mbr_digest;
    uint32_t open_handles;
    int *dispatched;
    const char *error;
};

static struct WriteTrial *write_trial;
#endif
static unsigned char write_check_sector[SECTOR_BYTES] __attribute__((aligned(64)));

static const char *write_media_state(const struct StockReader *reader,
                                     struct StockWriteState *observed)
{
    uintptr_t io = reader->data + IO_CONTEXT;
    uintptr_t media = reader->data + MEDIA_CONTEXT;
    struct StockWriteState state = {1, runtime_word(media + MEDIA_TYPE), runtime_word(io + 0x10),
                                    runtime_word(media + 0x4c18), runtime_word(media + 0x4c1c)};
    if (observed)
        *observed = state;
    if (!reader->reported_blocks || runtime_word(media + SECTOR_COUNT) != reader->reported_blocks)
        return "Stock sector count changed.";
    if ((state.media_type != 64 && state.media_type != 65) ||
        runtime_word(reader->data + ACTIVE_TABLE) != reader->text + PRO_TABLE ||
        runtime_word(reader->text + PRO_TABLE) != reader->text + PRO_READ ||
        runtime_word(reader->text + PRO_TABLE + 4) != reader->text + PRO_WRITE)
        return "Active Memory Stick PRO transfer path differs.";
    if ((int32_t)runtime_word(io) <= 0 ||
        runtime_word(io + 8) != media || runtime_word(reader->data + ACTIVE_MEDIA) != media ||
        !runtime_word(io + 4) || runtime_word(io + 4) != runtime_word(media + CONTROLLER))
        return "Stock I/O context changed or is unavailable.";
    if (state.write_protected)
        return "Stock driver reports write protection.";
    if (!state.write_enabled)
        return "Stock driver has disabled writes for this media.";
    if ((int32_t)state.open_handles <= 0)
        return "The stock I/O handle is no longer open.";
    return NULL;
}

static const char *check_write_media(const struct StockReader *reader, struct StockWriteState *observed)
{
    if (!matches_code(reader, RAW_WRITE, RAW_WRITE_END,
                      "\x56\x4b\xd3\x78\x15\xbb\x49\x25\x9f\xa0\x79\xe4\x54\xe0\x73\x27\xd4\x49\xf2\xd1") ||
        !matches_code(reader, PRO_WRITE, PRO_WRITE_END,
                      "\xa1\x59\xbf\xf5\x69\x8d\xc1\x86\x6a\x00\x30\xe5\x62\x0e\x0b\xd5\xe0\x48\xe7\x09"))
        return "Stock write code did not match.";
    return write_media_state(reader, observed);
}

#ifdef MSFORMAT_TRIAL
static unsigned char format_write_sector[SECTOR_BYTES] __attribute__((aligned(64)));
#endif

#ifndef MSWRITE_PATH_TRIAL
static const char *write_trial_state(const struct StockReader *reader, struct StockWriteState *observed)
{
    if (reader->reported_blocks != TRIAL_REPORTED_BLOCKS)
        return "Wrong trial adapter sector count.";
    return write_media_state(reader, observed);
}

const char *stock_write_trial_check(const struct StockReader *reader, struct StockWriteState *observed)
{
    if (reader->reported_blocks != TRIAL_REPORTED_BLOCKS)
        return "Wrong trial adapter sector count.";
    return check_write_media(reader, observed);
}

#ifndef MSFORMAT_TRIAL
static int dispatch_write_trial(uint32_t lba, unsigned count, void *buffer)
{
    struct WriteTrial *trial = write_trial;
    struct StockWriteState state;
    unsigned char digest[DIGEST_BYTES];
    ReadDispatch raw_read;
    if (!trial || lba != TRIAL_LBA || count != 1 || *trial->dispatched)
        return -1;
    trial->error = write_trial_state(trial->reader, &state);
    if (trial->error)
        return -1;
    if (state.open_handles != trial->open_handles) {
        trial->error = "Stock I/O handle count changed before the write.";
        return -1;
    }
    raw_read = (ReadDispatch)(trial->reader->text + RAW_DISPATCH);
    /* The stock worker owns its mutex across the identity checks and write.
       Its open-handle count includes idle clients of the same driver. */
    if (raw_read(0, 1, write_check_sector) != 0 ||
        sceKernelUtilsSha1Digest(write_check_sector, SECTOR_BYTES, digest) != 0 ||
        memcmp(digest, trial->mbr_digest, DIGEST_BYTES) ||
        raw_read(lba, 1, write_check_sector) != 0 ||
        memcmp(write_check_sector, trial->original, SECTOR_BYTES)) {
        trial->error = "Card identity or target data changed under the I/O lock.";
        return -1;
    }
    trial->error = write_trial_state(trial->reader, &state);
    if (trial->error)
        return -1;
    if (state.open_handles != trial->open_handles) {
        trial->error = "Stock I/O handle count changed before the write.";
        return -1;
    }
    *trial->dispatched = 1;
    return ((ReadDispatch)(trial->reader->text + RAW_WRITE))(lba, count, buffer);
}

const char *stock_write_trial(const struct StockReader *reader, const void *original,
                              void *replacement, const void *mbr_digest,
                              int *result, int *dispatched)
{
    struct StockWriteState state;
    const char *error = stock_write_trial_check(reader, &state);
    struct StockFile request = {0};
    struct WriteTrial trial = {.reader = reader, .original = original,
                               .mbr_digest = mbr_digest, .dispatched = dispatched};
    if (error)
        return error;
    *dispatched = 0;
    trial.open_handles = state.open_handles;
    request.file.arg = (void *)(reader->data + IO_CONTEXT);
    request.position = (uint64_t)TRIAL_LBA * SECTOR_BYTES;
    write_trial = &trial;
    *result = ((StockRead)(reader->text + STOCK_READ))(
        &request, replacement, SECTOR_BYTES, 1, dispatch_write_trial);
    write_trial = NULL;
    return trial.error;
}
#else
#ifdef MSDIRS_CARD_TRIAL
enum { FORMAT_METADATA_SECTORS = TRIAL_DIRECTORY_METADATA_SECTORS };
#else
enum { FORMAT_METADATA_SECTORS = 9694 };
#endif

struct LockedFormat {
    struct StockFormatTrial *trial;
    uint32_t open_handles;
#ifndef MSDIRS_CARD_TRIAL
    unsigned verify_events;
#endif
    int started;
};

static struct LockedFormat *active_format;
static int format_read(struct StockFormatTrial *trial, uint32_t lba)
{
    ReadDispatch read = (ReadDispatch)(trial->reader->text + RAW_DISPATCH);
    trial->command_result = read(lba, 1, write_check_sector);
    return trial->command_result == 0;
}

static int format_witness(struct StockFormatTrial *trial, uint32_t lba, unsigned index)
{
    unsigned char digest[DIGEST_BYTES];
    return format_read(trial, lba) &&
           !sceKernelUtilsSha1Digest(write_check_sector, SECTOR_BYTES, digest) &&
           !memcmp(digest, trial->reference->witnesses[index], sizeof(digest));
}

static int state_stable(struct LockedFormat *locked)
{
    struct StockWriteState state;
    struct StockFormatTrial *trial = locked->trial;
    trial->error = write_trial_state(trial->reader, &state);
    if (!trial->error && state.open_handles != locked->open_handles)
        trial->error = "Stock I/O handle count changed.";
    return !trial->error;
}

static int format_medium_matches(struct LockedFormat *locked)
{
    struct StockFormatTrial *trial = locked->trial;
    if (!state_stable(locked))
        return 0;
    if (!format_read(trial, 0) || memcmp(write_check_sector, trial->reference->marker, SECTOR_BYTES) ||
        !format_witness(trial, 2048, 1) || !format_witness(trial, 268435456, 2) ||
        !format_witness(trial, TRIAL_HIGH_LBA, 3) || !format_witness(trial, 230688768, 4) ||
        !format_witness(trial, TRIAL_SECTORS - 1, 6)) {
        trial->error = "The MBR or a control sector differs from the PC reference.";
        return 0;
    }
    return 1;
}

static int write_format_sector(void *context, uint32_t relative, const uint8_t *sector)
{
    struct LockedFormat *locked = context;
    struct StockFormatTrial *trial = locked->trial;
    if (relative >= FORMAT_METADATA_SECTORS ||
        trial->writes >= FORMAT_METADATA_SECTORS + 2) {
        trial->error = "A format write exceeded the approved metadata range.";
        return 0;
    }
    /* The existing worker owns the stock mutex throughout this callback. */
    if (!state_stable(locked))
        return 0;
    if (!format_witness(trial, TRIAL_HIGH_LBA, 3)) {
        trial->error = "The high-sector card witness changed before a write.";
        return 0;
    }
    if (!state_stable(locked))
        return 0;
    memcpy(format_write_sector, sector, SECTOR_BYTES);
    ReadDispatch write = (ReadDispatch)(trial->reader->text + RAW_WRITE);
    ++trial->writes;
    trial->command_result = write(TRIAL_FORMAT_START + relative, 1, format_write_sector);
    if (trial->command_result) {
        trial->error = "The stock sector write failed.";
        return 0;
    }
    return 1;
}

#ifdef MSDIRS_CARD_TRIAL
static int scan_directory_image(struct LockedFormat *locked, uint8_t *changed)
{
    struct StockFormatTrial *trial = locked->trial;
    uint8_t expected[SECTOR_BYTES];
    for (uint32_t relative = 0; relative < FORMAT_METADATA_SECTORS; ++relative) {
        if (!fat32_ram_read(trial->directory_image, relative, expected) ||
            !format_read(trial, TRIAL_FORMAT_START + relative)) {
            trial->error = changed ? "Could not read the complete metadata range before writing."
                                   : "Could not read back the new directory image.";
            return 0;
        }
        int differs = memcmp(write_check_sector, expected, SECTOR_BYTES);
        if (changed) {
            if (differs)
                changed[relative / 8] |= 1u << (relative % 8);
        } else {
            if (differs) {
                trial->error = "Directory image readback did not match.";
                return 0;
            }
            ++trial->verified;
        }
    }
    return 1;
}

static int publish_directory_image(struct LockedFormat *locked)
{
    struct StockFormatTrial *trial = locked->trial;
    uint8_t changed[(FORMAT_METADATA_SECTORS + 7) / 8] = {0};
    uint8_t expected[SECTOR_BYTES] = {0};
    if (!scan_directory_image(locked, changed) || !format_medium_matches(locked))
        return 0;
    if (!format_read(trial, TRIAL_FORMAT_START) ||
        memcmp(write_check_sector, trial->confirmed_boot, SECTOR_BYTES)) {
        trial->error = "The boot sector changed during the metadata scan. No writes made.";
        return 0;
    }
    /* Publish boot records only after the new FAT and directories are in place. */
    if (!write_format_sector(locked, 0, expected) || !write_format_sector(locked, 6, expected))
        return 0;
    for (uint32_t relative = 1; relative < FORMAT_METADATA_SECTORS; ++relative) {
        if (relative == 6 || !(changed[relative / 8] & (1u << (relative % 8))))
            continue;
        if (!fat32_ram_read(trial->directory_image, relative, expected) ||
            !write_format_sector(locked, relative, expected))
            return 0;
    }
    return fat32_ram_read(trial->directory_image, 6, expected) &&
           write_format_sector(locked, 6, expected) &&
           fat32_ram_read(trial->directory_image, 0, expected) &&
           write_format_sector(locked, 0, expected) &&
           format_medium_matches(locked) && scan_directory_image(locked, NULL) &&
           format_medium_matches(locked);
}
#else
static int verify_format_sector(void *context, uint32_t relative, const uint8_t *sector)
{
    struct LockedFormat *locked = context;
    struct StockFormatTrial *trial = locked->trial;
    /* The first two writes invalidate old boot sectors. Compare final contents. */
    if (locked->verify_events++ < 2)
        return 1;
    if (relative >= FORMAT_METADATA_SECTORS ||
        !format_read(trial, TRIAL_FORMAT_START + relative) ||
        memcmp(write_check_sector, sector, SECTOR_BYTES)) {
        trial->error = "Formatted metadata readback did not match.";
        return 0;
    }
    ++trial->verified;
    return 1;
}
#endif

static int dispatch_format_trial(uint32_t lba, unsigned count, void *buffer)
{
    struct LockedFormat *locked = active_format;
    if (!locked || locked->started || lba != TRIAL_FORMAT_START || count != 1 ||
        buffer != format_write_sector)
        return -1;
    locked->started = 1;
    struct StockFormatTrial *trial = locked->trial;
    if (!format_medium_matches(locked))
        return -1;
    if (!format_read(trial, TRIAL_FORMAT_START)) {
        trial->error = "Could not reread the confirmed boot sector under the I/O lock.";
        return -1;
    }
    if (memcmp(write_check_sector, trial->confirmed_boot, SECTOR_BYTES)) {
        trial->error = "The boot sector changed after confirmation. No writes made.";
        return -1;
    }
#ifdef MSDIRS_CARD_TRIAL
    if (!publish_directory_image(locked))
        return -1;
#else
    if (fat32_initialize(TRIAL_FORMAT_START, TRIAL_FORMAT_SECTORS, 0x50585031,
                         write_format_sector, locked) != FAT32_INIT_OK ||
        !format_medium_matches(locked) ||
        fat32_initialize(TRIAL_FORMAT_START, TRIAL_FORMAT_SECTORS, 0x50585031,
                         verify_format_sector, locked) != FAT32_INIT_OK ||
        !format_medium_matches(locked))
        return -1;
#endif
    return 0;
}

int stock_format_partition_trial(struct StockFormatTrial *trial)
{
    struct StockWriteState state;
    struct Fat32Layout layout;
    unsigned clusters = 1;
#ifdef MSDIRS_CARD_TRIAL
    clusters += PSP_INITIAL_DIRECTORY_COUNT;
    struct Fat32Ram *image = trial->directory_image;
    if (!image || image->start != TRIAL_FORMAT_START || image->length != TRIAL_FORMAT_SECTORS ||
        image->used > FAT32_RAM_CAPACITY || !fat32_ram_finish_directories(image)) {
        trial->error = "The completed Sony directory image did not verify.";
        return 0;
    }
    for (unsigned i = 0; i < image->used; ++i) {
        if (image->entries[i].lba >= FORMAT_METADATA_SECTORS) {
            trial->error = "The staged image contains data outside the metadata extent.";
            return 0;
        }
    }
#endif
    if (!fat32_plan(TRIAL_FORMAT_SECTORS, &layout) ||
        layout.root_sector + clusters * layout.sectors_per_cluster != FORMAT_METADATA_SECTORS ||
        !trial->confirmed_boot || active_format || trial->writes || trial->verified) {
        trial->error = "The formatter's metadata layout is unsupported.";
        return 0;
    }
    trial->error = stock_write_trial_check(trial->reader, &state);
    if (trial->error)
        return 0;
    struct LockedFormat locked = {.trial = trial, .open_handles = state.open_handles};
    struct StockFile request = {0};
    request.file.arg = (void *)(trial->reader->data + IO_CONTEXT);
    request.position = (uint64_t)TRIAL_FORMAT_START * SECTOR_BYTES;
    active_format = &locked;
    trial->worker_result = ((StockRead)(trial->reader->text + STOCK_READ))(
        &request, format_write_sector, SECTOR_BYTES, 1, dispatch_format_trial);
    active_format = NULL;
    if (!trial->error && trial->worker_result != SECTOR_BYTES)
        trial->error = "The stock format worker did not complete.";
    return !trial->error && trial->worker_result == SECTOR_BYTES &&
#ifdef MSDIRS_CARD_TRIAL
           trial->writes >= 4 &&
#else
           trial->writes == FORMAT_METADATA_SECTORS + 2 &&
#endif
           trial->verified == FORMAT_METADATA_SECTORS;
}
#endif
#endif

#ifdef MSMULTI_CARD_TRIAL
struct StockFormatAccess {
    const struct StockReader *reader;
    StockFormatOperation operation;
    void *context;
    uint32_t open_handles, sector_limit;
    int started;
};

static struct StockFormatAccess *transaction;

static int transaction_live(struct StockFormatAccess *access, uint32_t lba)
{
    struct StockWriteState state;
    return access && access == transaction && access->started && lba < access->sector_limit &&
           !write_media_state(access->reader, &state) && state.open_handles == access->open_handles;
}

int stock_format_locked_read(struct StockFormatAccess *access, uint32_t lba, void *buffer)
{
    if (!transaction_live(access, lba))
        return 0;
    ReadDispatch read = (ReadDispatch)(access->reader->text + RAW_DISPATCH);
    if (read(lba, 1, write_check_sector) != 0)
        return 0;
    memcpy(buffer, write_check_sector, SECTOR_BYTES);
    return transaction_live(access, lba);
}

int stock_format_locked_write_status(struct StockFormatAccess *access, uint32_t lba,
                                     const void *buffer, int *command)
{
    if (!transaction_live(access, lba))
        return 0;
    memcpy(format_write_sector, buffer, SECTOR_BYTES);
    ReadDispatch write = (ReadDispatch)(access->reader->text + RAW_WRITE);
    int result = write(lba, 1, format_write_sector);
    if (command)
        *command = result;
    return result == 0 && transaction_live(access, lba);
}

int stock_format_locked_write(struct StockFormatAccess *access, uint32_t lba, const void *buffer)
{
    return stock_format_locked_write_status(access, lba, buffer, NULL);
}

static int run_format_transaction(uint32_t lba, unsigned count, void *buffer)
{
    struct StockFormatAccess *access = transaction;
    if (!access || access->started || lba || count != 1 || buffer != format_write_sector)
        return -1;
    access->started = 1;
    access->operation(access, access->context);
    /* The caller owns operation status. Never retry a partly completed format. */
    return 0;
}

int stock_format_transaction(const struct StockReader *reader, StockFormatOperation operation,
                             void *context, uint32_t sector_limit)
{
    struct StockWriteState state;
    if (!operation || transaction || sector_limit < reader->reported_blocks ||
        sector_limit > CAPACITY_LIMIT_BYTES / SECTOR_BYTES || check_write_media(reader, &state))
        return 0;
    struct StockFormatAccess access = {reader, operation, context, state.open_handles, sector_limit, 0};
    struct StockFile request = {0};
    request.file.arg = (void *)(reader->data + IO_CONTEXT);
    transaction = &access;
    int result = ((StockRead)(reader->text + STOCK_READ))(
        &request, format_write_sector, SECTOR_BYTES, 1, run_format_transaction);
    transaction = NULL;
    return access.started ? (result == SECTOR_BYTES ? 1 : -1) : 0;
}

#ifndef MSWRITE_PATH_TRIAL
int stock_format_refresh_media(const struct StockReader *reader)
{
    /* Sony FAT issues this control after successful media formatting. Its PRO
       handler reloads the attributes and first MBR entry into driver state. */
    if (transaction || !reader->reported_blocks ||
        runtime_word(reader->data + MEDIA_CONTEXT + SECTOR_COUNT) != reader->reported_blocks ||
        runtime_word(reader->data + ACTIVE_TABLE) != reader->text + PRO_TABLE ||
        runtime_word(reader->text + PRO_TABLE + 12) != reader->text + 0x43b8 ||
        runtime_word(reader->text + 0x1f24) != 0x3c020210 ||
        runtime_word(reader->text + 0x1f28) != 0x3442d816 ||
        runtime_word(reader->text + 0x1f2c) != 0x12020061 ||
        runtime_word(reader->text + 0x3e14) != 0x920301be ||
        runtime_word(reader->text + 0x3f00) != 0xae514c04)
        return -1;
    return sceIoDevctl("msstor0:", 0x0210d816, NULL, 0, NULL, 0);
}
#endif
#endif
#endif
