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
#ifdef MSDIAG_WRITE_TRIAL
#include "write_trial.h"
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

const char *stock_read_sector(const struct StockReader *reader, uint32_t lba,
                              void *buffer, int *result, struct StockReadTrace *trace)
{
    uintptr_t io = reader->data + IO_CONTEXT;
    uintptr_t media = reader->data + MEDIA_CONTEXT;
    uint32_t type = runtime_word(media + MEDIA_TYPE);
    struct StockFile request = {0};
    StockRead read = (StockRead)(reader->text + STOCK_READ);
    ReadDispatch dispatch = (ReadDispatch)(reader->text + RAW_DISPATCH);
    struct TracedRead traced = {dispatch, trace, lba};
    if ((type != 64 && type != 65) ||
        runtime_word(reader->data + ACTIVE_TABLE) != reader->text + PRO_TABLE ||
        runtime_word(reader->text + PRO_TABLE) != reader->text + PRO_READ)
        return "Active Memory Stick PRO read path differs.";
    if ((int32_t)runtime_word(io) <= 0 || !runtime_word(io + 0x10) ||
        runtime_word(io + 8) != media || runtime_word(reader->data + ACTIVE_MEDIA) != media ||
        !runtime_word(io + 4) || runtime_word(io + 4) != runtime_word(media + CONTROLLER) ||
        runtime_word(media + SECTOR_COUNT) != reader->reported_blocks)
        return "Stock I/O context or media changed.";
    request.file.arg = (void *)io;
    request.position = (uint64_t)lba * SECTOR_BYTES;
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

#ifdef MSDIAG_WRITE_TRIAL
enum {
    RAW_WRITE = 0x32cc,
    RAW_WRITE_END = 0x3334,
    PRO_WRITE = 0x7b64,
    PRO_WRITE_END = 0x7d78,
};

struct WriteTrial {
    const struct StockReader *reader;
    const void *original;
    const void *mbr_digest;
    uint32_t open_handles;
    int *dispatched;
    const char *error;
};

static struct WriteTrial *write_trial;
static unsigned char write_check_sector[SECTOR_BYTES] __attribute__((aligned(64)));

static const char *write_trial_state(const struct StockReader *reader,
                                     struct StockWriteState *observed)
{
    uintptr_t io = reader->data + IO_CONTEXT;
    uintptr_t media = reader->data + MEDIA_CONTEXT;
    struct StockWriteState state = {1, runtime_word(media + MEDIA_TYPE), runtime_word(io + 0x10),
                                    runtime_word(media + 0x4c18), runtime_word(media + 0x4c1c)};
    if (observed)
        *observed = state;
    if (reader->reported_blocks != TRIAL_REPORTED_BLOCKS ||
        runtime_word(media + SECTOR_COUNT) != TRIAL_REPORTED_BLOCKS)
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

const char *stock_write_trial_check(const struct StockReader *reader, struct StockWriteState *observed)
{
    if (!matches_code(reader, RAW_WRITE, RAW_WRITE_END,
                      "\x56\x4b\xd3\x78\x15\xbb\x49\x25\x9f\xa0\x79\xe4\x54\xe0\x73\x27\xd4\x49\xf2\xd1") ||
        !matches_code(reader, PRO_WRITE, PRO_WRITE_END,
                      "\xa1\x59\xbf\xf5\x69\x8d\xc1\x86\x6a\x00\x30\xe5\x62\x0e\x0b\xd5\xe0\x48\xe7\x09"))
        return "Stock write code did not match.";
    return write_trial_state(reader, observed);
}

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
#endif
