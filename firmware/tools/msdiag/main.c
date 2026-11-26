/* Read-only PSP Memory Stick diagnostic. GPL-3.0-or-later. */
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspsysmem_kernel.h>
#include <psputils.h>
#include <string.h>
#include <systemctrl.h>

#include "capacity.h"
#include "probe.h"
#include "stock_reader.h"

PSP_MODULE_INFO("ProxMsDiag", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#define REFERENCE_PATH "ms0:/PSP/GAME/MSDIAG/MSDREF.BIN"
#define RAW_DEVICE "msstor:"
#define VOLUME_DEVICE "ms0:"
#define MOUNTED_CAPACITY 0x02425818
/* PRO-C's USB reader uses this value as a count of 512-byte blocks. */
#define RAW_USB_CAPACITY 0x02125006
/* The stock driver copies its cached 96-byte media-information record. */
#define RAW_MEDIA_ATTRIBUTES 0x02125803
#define TRANSFER_FAILURE 0x8022008au

/* SCE_PR_GETDEV layout for the pinned SDK, which predates the named API. */
struct VolumeCapacity {
    uint32_t max_clusters, free_clusters, max_sectors;
    int32_t sector_size, sectors_per_cluster;
};

_Static_assert(sizeof(struct VolumeCapacity) == 20, "SCE_PR_GETDEV ABI");
_Static_assert(sizeof(SceOff) == 8, "Must use the 64-bit SDK seek");

static const uint64_t wrap_periods[] = {
    UINT64_C(1) << 23, DECIMAL_CUTOFF, BINARY_CUTOFF
};
static const char *const wrap_labels[] = {"4 GiB", "128 GB", "128 GiB"};
#define WRAP_COUNT (sizeof(wrap_periods) / sizeof(wrap_periods[0]))

struct Probe {
    uint64_t lba;
    struct SectorRead read;
    struct SectorRead aliases[WRAP_COUNT];
    size_t alias_count;
};

static const char *const labels[PROBE_COUNT] = {
    "Sector zero", "Low control", "Before 128 GB", "At/above 128 GB",
    "Before 128 GiB", "At/above 128 GiB", "Near card end", "Last sector"
};
static unsigned char sector[SECTOR_BYTES] __attribute__((aligned(64)));
static struct Reference reference;
static struct Probe probes[PROBE_COUNT];
static struct VolumeCapacity volume;
static int reference_status, identity_ok, volume_result, raw_open, raw_close;
static int raw_capacity_result;
static uint32_t raw_capacity_blocks;
static SceOff raw_end;
static struct MediaGeometry media_geometry;
static uint64_t layout_end;
static int attributes_result, attributes_class;
static struct SectorRead transfer_low;
static struct Probe transfer_high;
static const char *transfer_status = "Matching direct-reader reference required.";
static int transfer_low_attempted, transfer_high_attempted;

struct BoundaryRead {
    uint32_t lba;
    struct SectorRead read;
    struct StockReadTrace trace;
    const char *blocked;
};

static struct BoundaryRead last_inside, first_outside, recovery;
static const char *boundary_status = "Requires a matching high-sector sample.";

enum { MAX_ESTIMATE_READS = CAPACITY_LIMIT_BYTES / SECTOR_BYTES / COUNT_WRAP_SECTORS + 5 };
struct EstimateRead {
    uint32_t lba;
    int result, command;
    unsigned calls;
};

static struct CapacityEstimate estimate;
static struct EstimateRead estimate_reads[MAX_ESTIMATE_READS];
static size_t estimate_read_count;
static const char *estimate_error;
static uint64_t estimate_elapsed;

static int load_reference(void)
{
    uint8_t data[REFERENCE_BYTES + 1];
    SceUID fd = sceIoOpen(REFERENCE_PATH, PSP_O_RDONLY, 0);
    size_t used = 0;
    int count = 0;
    if (fd < 0)
        return fd == (int)0x80010002 ? 0 : -1;
    while (used < sizeof(data)) {
        count = sceIoRead(fd, data + used, sizeof(data) - used);
        if (count <= 0)
            break;
        used += count;
    }
    if (sceIoClose(fd) < 0 || count < 0)
        return -1;
    return decode_reference(&reference, data, used) ? 1 : -1;
}

static uint64_t volume_bytes(uint32_t clusters)
{
    if (volume_result < 0 || volume.sector_size < 512 || volume.sector_size > 65536 ||
        volume.sectors_per_cluster < 1 || volume.sectors_per_cluster > 65536)
        return 0;
    return (uint64_t)clusters * volume.sector_size * volume.sectors_per_cluster;
}

static void hash_sector(struct SectorRead *result)
{
    size_t i;
    result->hash_result = sceKernelUtilsSha1Digest(sector, sizeof(sector), result->digest);
    for (i = 1; i < sizeof(sector) && sector[i] == sector[0]; i++)
        ;
    result->uniform = i == sizeof(sector);
}

static void read_sector(SceUID fd, uint64_t lba, struct SectorRead *result)
{
    SceOff offset = (SceOff)(lba * SECTOR_BYTES);
    result->bytes = -1;
    result->seek = sceIoLseek(fd, offset, PSP_SEEK_SET);
    if (result->seek != offset)
        return;
    result->bytes = sceIoRead(fd, sector, sizeof(sector));
    if (result->bytes == SECTOR_BYTES)
        hash_sector(result);
}

static void collect_aliases(SceUID fd, struct Probe *probe)
{
    size_t i;
    for (i = 0; i < WRAP_COUNT && probe->lba >= wrap_periods[i]; i++) {
        read_sector(fd, probe->lba % wrap_periods[i], &probe->aliases[i]);
        probe->alias_count++;
    }
}

static void read_boundary_sector(const struct StockReader *reader, struct BoundaryRead *sample,
                                 uint32_t lba)
{
    sample->lba = lba;
    pspDebugScreenPrintf("Boundary test at LBA %u.\n", (unsigned)lba);
    sample->blocked = stock_read_sector(reader, lba, sector, &sample->read.bytes, &sample->trace);
    if (!sample->blocked && sample->read.bytes == SECTOR_BYTES)
        hash_sector(&sample->read);
}

static void collect_boundary(const struct StockReader *reader, const struct Sample *high)
{
    const struct Sample *last = &reference.sample[LAST_SECTOR];
    if (reference.bytes > CAPACITY_LIMIT_BYTES) {
        boundary_status = "PC reference exceeds the 1 TB test limit.";
        return;
    }
    read_boundary_sector(reader, &last_inside, last->lba);
    if (last_inside.blocked || !matches_reference(&last_inside.read, last)) {
        boundary_status = "Last-sector control failed. Outside read skipped.";
        return;
    }
    read_boundary_sector(reader, &first_outside, last->lba + 1);
    if (first_outside.blocked) {
        boundary_status = "Outside read blocked before reaching the worker.";
        return;
    }
    read_boundary_sector(reader, &recovery, high->lba);
    if (recovery.blocked || !matches_reference(&recovery.read, high)) {
        boundary_status = "High control did not recover after outside read.";
    } else if (first_outside.trace.calls && first_outside.read.bytes < 0) {
        boundary_status = "Outside read failed. High control recovered.";
    } else {
        boundary_status = "Outside result inconclusive. High control recovered.";
    }
}

static int consistent_geometry(void)
{
    return raw_capacity_result >= 0 && raw_capacity_blocks &&
           raw_end == (SceOff)raw_capacity_blocks * SECTOR_BYTES &&
           media_geometry.bytes == (uint64_t)raw_end &&
           media_geometry.unit_bytes == SECTOR_BYTES;
}

static void collect_transfer_probe(SceUID fd)
{
    struct StockReader reader;
    const struct Sample *low = &reference.sample[LOW_CONTROL];
    const struct Sample *high = &reference.sample[ABOVE_BINARY];
    if (!identity_ok)
        return;
    if (high->lba == NO_LBA || high->lba > UINT32_MAX ||
        high->lba < raw_capacity_blocks || !consistent_geometry()) {
        transfer_status = "Need consistent report and a PC sample above it.";
        return;
    }
    transfer_status = stock_reader_init(&reader, raw_capacity_blocks);
    if (transfer_status)
        return;
    pspDebugScreenPrintf("Testing stock transfer at low LBA %u.\n", (unsigned)low->lba);
    transfer_status = stock_read_sector(&reader, low->lba, sector, &transfer_low.bytes, NULL);
    if (transfer_status)
        return;
    transfer_low_attempted = 1;
    if (transfer_low.bytes == SECTOR_BYTES)
        hash_sector(&transfer_low);
    if (!matches_reference(&transfer_low, low) || transfer_low.uniform) {
        transfer_status = "Low control failed. High transfer skipped.";
        return;
    }
    transfer_high.lba = high->lba;
    collect_aliases(fd, &transfer_high);
    pspDebugScreenPrintf("Testing stock transfer at high LBA %u.\n", (unsigned)high->lba);
    transfer_status = stock_read_sector(&reader, high->lba, sector, &transfer_high.read.bytes, NULL);
    if (transfer_status)
        return;
    transfer_high_attempted = 1;
    if (transfer_high.read.bytes == SECTOR_BYTES)
        hash_sector(&transfer_high.read);
    transfer_status = sample_result(identity_ok, &transfer_high.read, high,
                                    transfer_high.aliases, transfer_high.alias_count);
    if (matches_reference(&transfer_high.read, high) && !transfer_high.read.uniform)
        collect_boundary(&reader, high);
}

static int live_control_matches(const struct StockReader *reader)
{
    const struct Probe *control = &probes[LOW_CONTROL];
    struct SectorRead result = {0};
    if (!sector_read_ok(&control->read) ||
        stock_read_sector(reader, control->lba, sector, &result.bytes, NULL) ||
        result.bytes != SECTOR_BYTES)
        return 0;
    hash_sector(&result);
    return sector_read_ok(&result) && !memcmp(result.digest, control->read.digest, DIGEST_BYTES);
}

static enum CapacityReply read_estimate_sector(void *context, uint32_t lba)
{
    struct EstimateRead *log;
    struct StockReadTrace trace;
    int result = -1;
    pspDebugScreenPrintf("Capacity test at LBA %u.\n", (unsigned)lba);
    estimate_error = stock_read_sector(context, lba, sector, &result, &trace);
    if (estimate_error)
        return CAPACITY_OTHER_FAILURE;
    if (estimate_read_count < MAX_ESTIMATE_READS) {
        log = &estimate_reads[estimate_read_count++];
        *log = (struct EstimateRead){lba, result, trace.last_result, trace.calls};
    }
    if (trace.calls != 1)
        return CAPACITY_OTHER_FAILURE;
    if (result == SECTOR_BYTES && trace.last_result == 0)
        return CAPACITY_READ;
    if ((uint32_t)result == TRANSFER_FAILURE && (uint32_t)trace.last_result == TRANSFER_FAILURE)
        return CAPACITY_TRANSFER_FAILURE;
    return CAPACITY_OTHER_FAILURE;
}

static void collect_capacity_estimate(void)
{
    struct StockReader reader;
    uint64_t started;
    if (!consistent_geometry() || media_geometry.block_size != 4096) {
        estimate_error = "Expected consistent 4096-sector blocks.";
        return;
    }
    estimate_error = stock_reader_init(&reader, raw_capacity_blocks);
    if (estimate_error)
        return;
    if (!live_control_matches(&reader)) {
        estimate_error = "Low control differed from the normal read.";
        return;
    }
    started = sceKernelGetSystemTimeWide();
    estimate = estimate_capacity(raw_capacity_blocks, read_estimate_sector, &reader);
    if (estimate.finding != CAPACITY_INCONCLUSIVE && !live_control_matches(&reader)) {
        estimate.finding = CAPACITY_INCONCLUSIVE;
        estimate_error = "Low control failed after the probe.";
    }
    estimate_elapsed = sceKernelGetSystemTimeWide() - started;
}

static void scan_card(int use_reference)
{
    struct VolumeCapacity *capacity = &volume;
    uint8_t attributes[MEDIA_ATTRIBUTE_BYTES] __attribute__((aligned(64))) = {0};
    uint64_t sectors = 0;
    size_t i;
    volume_result = sceIoDevctl(VOLUME_DEVICE, MOUNTED_CAPACITY, &capacity,
                                sizeof(capacity), NULL, 0);
    raw_open = sceIoOpen(RAW_DEVICE, PSP_O_RDONLY, 0);
    if (raw_open < 0)
        return;
    raw_capacity_result = sceIoIoctl(raw_open, RAW_USB_CAPACITY, NULL, 0,
                                    &raw_capacity_blocks, sizeof(raw_capacity_blocks));
    attributes_result = sceIoIoctl(raw_open, RAW_MEDIA_ATTRIBUTES, NULL, 0,
                                  attributes, sizeof(attributes));
    if (attributes_result >= 0) {
        attributes_class = attributes[0];
        decode_media_geometry(&media_geometry, attributes);
    }
    raw_end = sceIoLseek(raw_open, 0, PSP_SEEK_END);
    if (raw_end > 0 && raw_end % SECTOR_BYTES == 0)
        sectors = (uint64_t)raw_end / SECTOR_BYTES;
    else if (raw_capacity_result >= 0)
        sectors = raw_capacity_blocks;
    for (i = 0; i < PROBE_COUNT; i++) {
        struct Probe *probe = &probes[i];
        probe->lba = use_reference && reference_status == 1 ?
                     reference.sample[i].lba : default_lba(i, sectors);
        if (probe->lba == NO_LBA)
            continue;
        pspDebugScreenPrintf("Reading %s (LBA %llu)\n", labels[i],
                             (unsigned long long)probe->lba);
        read_sector(raw_open, probe->lba, &probe->read);
        if (sector_read_ok(&probe->read)) {
            if (i == SECTOR_ZERO)
                layout_end = primary_mbr_end(sector);
            collect_aliases(raw_open, probe);
        }
    }
    identity_ok = use_reference && reference_status == 1 &&
                  matches_reference(&probes[SECTOR_ZERO].read, &reference.sample[SECTOR_ZERO]) &&
                  matches_reference(&probes[LOW_CONTROL].read, &reference.sample[LOW_CONTROL]) &&
                  (!probes[SECTOR_ZERO].read.uniform || !probes[LOW_CONTROL].read.uniform);
    if (use_reference)
        collect_transfer_probe(raw_open);
    else
        collect_capacity_estimate();
    raw_close = sceIoClose(raw_open);
}

static void show_summary(int use_reference)
{
    uint64_t total = volume_bytes(volume.max_clusters);
    uint64_t free = volume_bytes(volume.free_clusters);
    pspDebugScreenPrintf("PSP model ID %d  System software 0x%08x\n\n",
                         sceKernelGetModel(), (unsigned)sceKernelDevkitVersion());
    pspDebugScreenPrintf("Mounted %s capacity query: 0x%08x\n", VOLUME_DEVICE,
                         (unsigned)volume_result);
    if (total) {
        pspDebugScreenPrintf("  Total %llu  Free %llu bytes\n", (unsigned long long)total,
                             (unsigned long long)free);
        pspDebugScreenPrintf("  Sector %d B  Sectors per cluster %d\n",
                             volume.sector_size, volume.sectors_per_cluster);
    }
    pspDebugScreenPrintf("\nRaw %s (512 B reads)\n", RAW_DEVICE);
    if (raw_open < 0) {
        pspDebugScreenPrintf("  Open failed: 0x%08x\n", (unsigned)raw_open);
    } else {
        if (raw_capacity_result >= 0 && raw_capacity_blocks)
            pspDebugScreenPrintf("  USB size query: %u blocks / %llu bytes\n",
                                 (unsigned)raw_capacity_blocks,
                                 (unsigned long long)raw_capacity_blocks * SECTOR_BYTES);
        else
            pspDebugScreenPrintf("  USB size query unavailable: 0x%08x\n",
                                 (unsigned)raw_capacity_result);
        if (raw_end > 0 && raw_end % SECTOR_BYTES == 0)
            pspDebugScreenPrintf("  SEEK_END reports %lld bytes\n", (long long)raw_end);
        else
            pspDebugScreenPrintf("  SEEK_END unavailable: %lld (0x%08x)\n",
                                 (long long)raw_end, (unsigned)raw_end);
        pspDebugScreenPrintf("  Close: 0x%08x\n", (unsigned)raw_close);
        if (media_geometry.bytes) {
            pspDebugScreenPrintf("\nMemory Stick attributes: %llu bytes\n",
                                 (unsigned long long)media_geometry.bytes);
            pspDebugScreenPrintf("  Block %u, user blocks %u, unit %u B\n",
                                 media_geometry.block_size, media_geometry.user_blocks,
                                 media_geometry.unit_bytes);
        } else {
            pspDebugScreenPrintf("\nMemory Stick attributes unavailable\n"
                                 "  Query 0x%08x  class %d\n",
                                 (unsigned)attributes_result, attributes_class);
        }
        if (layout_end)
            pspDebugScreenPrintf("MBR declared end: %llu bytes (exclusive)\n",
                                 (unsigned long long)layout_end * SECTOR_BYTES);
    }
    if (!use_reference)
        pspDebugScreenPrintf("\nAutomatic estimate. Triangle for result.\n");
    else if (reference_status == 1)
        pspDebugScreenPrintf("\nPC reader: %llu bytes, 512 B sectors\n  Controls: %s\n",
                             (unsigned long long)reference.bytes,
                             identity_ok ? "match" : "unconfirmed");
    else
        pspDebugScreenPrintf("\nPC reference: %s\n", reference_status ? "invalid/unreadable" : "absent");
    pspDebugScreenPrintf("\nUse Right for each sector result. Take photos.\n");
}

static void show_read(uint64_t lba, const struct SectorRead *result)
{
    size_t i;
    pspDebugScreenPrintf("LBA %llu\n", (unsigned long long)lba);
    if (!sector_read_ok(result)) {
        pspDebugScreenPrintf("Seek %lld (0x%08x), read %d (0x%08x)\n",
                             (long long)result->seek, (unsigned)result->seek,
                             result->bytes, (unsigned)result->bytes);
        if (result->bytes == SECTOR_BYTES)
            pspDebugScreenPrintf("Hash error: 0x%08x\n", (unsigned)result->hash_result);
        return;
    }
    pspDebugScreenPrintf("Read 512 bytes; SHA-1 ");
    for (i = 0; i < DIGEST_BYTES; i++)
        pspDebugScreenPrintf("%02x", result->digest[i]);
    pspDebugScreenPrintf("\n");
}

static void show_probe(size_t index, int use_reference)
{
    const struct Probe *probe = &probes[index];
    const struct Sample *expected = use_reference && reference_status == 1 ?
                                    &reference.sample[index] : NULL;
    size_t i;
    pspDebugScreenPrintf("%s\n\n", labels[index]);
    if (probe->lba == NO_LBA) {
        pspDebugScreenPrintf("Skipped: range unavailable for this geometry.\n");
        return;
    }
    show_read(probe->lba, &probe->read);
    pspDebugScreenPrintf("\n%s\n", sample_result(identity_ok, &probe->read, expected,
                                                  probe->aliases, probe->alias_count));
    if (expected) {
        pspDebugScreenPrintf("PC SHA-1 ");
        for (i = 0; i < DIGEST_BYTES; i++)
            pspDebugScreenPrintf("%02x", expected->digest[i]);
        pspDebugScreenPrintf("\n");
    }
    for (i = 0; i < probe->alias_count; i++) {
        pspDebugScreenPrintf("\nLower LBA for %s wraparound\n", wrap_labels[i]);
        show_read(probe->lba % wrap_periods[i], &probe->aliases[i]);
    }
}

static void show_transfer_result(const char *label, const struct Sample *expected,
                                 const struct SectorRead *read)
{
    size_t i;
    pspDebugScreenPrintf("\n%s\n", label);
    if (sector_read_ok(read))
        show_read(expected->lba, read);
    else
        pspDebugScreenPrintf("LBA %llu, result %d (0x%08x)\n",
                             (unsigned long long)expected->lba, read->bytes,
                             (unsigned)read->bytes);
    if (read->bytes == SECTOR_BYTES && read->hash_result)
        pspDebugScreenPrintf("Hash error: 0x%08x\n", (unsigned)read->hash_result);
    pspDebugScreenPrintf("PC SHA-1 ");
    for (i = 0; i < DIGEST_BYTES; i++)
        pspDebugScreenPrintf("%02x", expected->digest[i]);
    pspDebugScreenPrintf("\n");
}

static void show_transfer(void)
{
    pspDebugScreenPrintf("Stock 6.61 transfer test (read only)\n\n%s\n", transfer_status);
    if (transfer_low_attempted)
        show_transfer_result("Low control", &reference.sample[LOW_CONTROL], &transfer_low);
    if (transfer_high_attempted)
        show_transfer_result("High sample", &reference.sample[ABOVE_BINARY], &transfer_high.read);
}

static void show_boundary_read(const char *label, const struct BoundaryRead *sample)
{
    size_t i;
    if (!sample->lba)
        return;
    pspDebugScreenPrintf("\n%s  LBA %u\n", label, (unsigned)sample->lba);
    if (sample->blocked) {
        pspDebugScreenPrintf("%s\n", sample->blocked);
        return;
    }
    pspDebugScreenPrintf("Worker %d (0x%08x), commands %u\n",
                         sample->read.bytes, (unsigned)sample->read.bytes, sample->trace.calls);
    if (sample->trace.calls)
        pspDebugScreenPrintf("Command first 0x%08x  last 0x%08x\n",
                             (unsigned)sample->trace.first_result, (unsigned)sample->trace.last_result);
    if (sector_read_ok(&sample->read)) {
        pspDebugScreenPrintf("SHA-1 ");
        for (i = 0; i < DIGEST_BYTES; i++)
            pspDebugScreenPrintf("%02x", sample->read.digest[i]);
        pspDebugScreenPrintf("\n");
    }
}

static void show_boundary(void)
{
    pspDebugScreenPrintf("Card boundary test (read only)\n\n%s\n", boundary_status);
    show_boundary_read("Last sector inside", &last_inside);
    if (!last_inside.blocked && last_inside.lba &&
        matches_reference(&last_inside.read, &reference.sample[LAST_SECTOR]))
        pspDebugScreenPrintf("PC match%s\n", last_inside.read.uniform ? " (uniform data)" : "");
    show_boundary_read("First sector outside", &first_outside);
    show_boundary_read("High control after outside read", &recovery);
    if (!recovery.blocked && recovery.lba &&
        matches_reference(&recovery.read, &reference.sample[ABOVE_BINARY]))
        pspDebugScreenPrintf("PC match\n");
}

static void show_estimate(void)
{
    size_t i;
    pspDebugScreenPrintf("Automatic capacity estimate (read only)\n\n");
    if (estimate.finding == CAPACITY_CANDIDATE) {
        uint64_t bytes = (uint64_t)estimate.sectors * SECTOR_BYTES;
        unsigned gib100 = bytes * 100 / (UINT64_C(1) << 30);
        pspDebugScreenPrintf("Candidate %llu bytes (%u.%02u GiB)\nEnd before LBA %u\n",
                             (unsigned long long)bytes, gib100 / 100, gib100 % 100,
                             (unsigned)estimate.sectors);
    } else if (estimate.finding == CAPACITY_CAP_SAMPLE) {
        pspDebugScreenPrintf("Read returned data at the 1 TB check.\nActual size unconfirmed.\n");
    } else {
        pspDebugScreenPrintf("Size inconclusive.\n");
        if (estimate_error)
            pspDebugScreenPrintf("%s\n", estimate_error);
    }
    if (estimate_read_count)
        pspDebugScreenPrintf("\nElapsed %llu ms\nLBA            Worker      Command     Calls\n",
                             (unsigned long long)(estimate_elapsed / 1000));
    for (i = 0; i < estimate_read_count; ++i) {
        const struct EstimateRead *log = &estimate_reads[i];
        pspDebugScreenPrintf("%-12u   %08x    %08x    %u\n", (unsigned)log->lba,
                             (unsigned)log->result, (unsigned)log->command, log->calls);
    }
}

static unsigned next_button(void)
{
    static unsigned previous = PSP_CTRL_CROSS;
    SceCtrlData pad = {0};
    for (;;) {
        if (sceCtrlPeekBufferPositive(&pad, 1) > 0) {
            unsigned pressed = pad.Buttons & ~previous;
            previous = pad.Buttons;
            if (pressed)
                return pressed;
        }
        sceKernelDelayThread(20000);
    }
}

int main(void)
{
    unsigned button;
    size_t page = 0, last_page;
    int exit_result, use_reference;
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenPrintf("PRO-X Memory Stick Diagnostic\n%s\n\n", MSDIAG_BUILD_ID);
    reference_status = load_reference();
    pspDebugScreenPrintf("PC reference: %s\n\n",
                         reference_status == 1 ? "loaded" :
                         reference_status == 0 ? "absent" : "invalid/unreadable");
    pspDebugScreenPrintf("Leave the test card inserted, or swap now\n"
                         "while the Memory Stick light is off.\n"
                         "X: automatic estimate (read only)\n"
                         "Square: PC comparison    O: exit\n");
    do {
        button = next_button();
        if (button & PSP_CTRL_CIRCLE)
            goto exit;
    } while (!(button & (PSP_CTRL_CROSS | PSP_CTRL_SQUARE)));
    use_reference = (button & PSP_CTRL_SQUARE) != 0;
    pspDebugScreenClear();
    pspDebugScreenPrintf("Reading card. Leave it inserted.\n\n");
    scan_card(use_reference);
    last_page = raw_open < 0 ? 0 : PROBE_COUNT + 1 + use_reference;
    do {
        pspDebugScreenClear();
        pspDebugScreenPrintf("PRO-X MSDIAG %s   Page %u/%u\n\n", MSDIAG_BUILD_ID,
                             (unsigned)page + 1, (unsigned)last_page + 1);
        if (!use_reference && page == PROBE_COUNT + 1)
            show_estimate();
        else if (page == PROBE_COUNT + 2)
            show_boundary();
        else if (page == PROBE_COUNT + 1)
            show_transfer();
        else if (page)
            show_probe(page - 1, use_reference);
        else
            show_summary(use_reference);
        pspDebugScreenPrintf("\nLeft/Right: pages    Triangle: last    O: exit\n");
        do {
            button = next_button();
        } while (!(button & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)));
        if ((button & PSP_CTRL_RIGHT) && page < last_page)
            page++;
        if ((button & PSP_CTRL_LEFT) && page)
            page--;
        if (button & PSP_CTRL_TRIANGLE)
            page = last_page;
    } while (!(button & PSP_CTRL_CIRCLE));
exit:
    /* The reference and raw card descriptors are closed before returning. */
    pspDebugScreenPrintf("\nReturning to XMB...\n");
    exit_result = sctrlKernelExitVSH(NULL);
    pspDebugScreenPrintf("Return to XMB failed: 0x%08x\n", (unsigned)exit_result);
    return 1;
}
