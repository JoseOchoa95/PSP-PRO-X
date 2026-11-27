/* GPL-3.0-or-later */
#include <pspiofilemgr_kernel.h>
#include <string.h>

#include "psp_media.h"

enum { CAPACITY_IOCTL = 0x02125006, ATTRIBUTES_IOCTL = 0x02125803 };
static uint8_t transfer[SECTOR_BYTES] __attribute__((aligned(64)));

static enum CapacityReply read_stock(void *context, uint32_t lba, uint8_t *out)
{
    struct StockReadTrace trace;
    int bytes;
    if (stock_read_sector(context, lba, transfer, &bytes, &trace) || trace.calls != 1)
        return CAPACITY_OTHER_FAILURE;
    if (bytes == SECTOR_BYTES && !trace.last_result) {
        memcpy(out, transfer, SECTOR_BYTES);
        return CAPACITY_READ;
    }
    if ((uint32_t)bytes == 0x8022008a && (uint32_t)trace.last_result == 0x8022008a)
        return CAPACITY_TRANSFER_FAILURE;
    return CAPACITY_OTHER_FAILURE;
}

int ms_auto_read_sector(void *reader, uint32_t lba, uint8_t *out)
{
    return read_stock(reader, lba, out) == CAPACITY_READ;
}

int ms_auto_check_boundary(struct StockReader *reader, uint32_t end)
{
    uint8_t sector[SECTOR_BYTES];
    return end > reader->reported_blocks && ms_auto_read_sector(reader, end - 1, sector) &&
           read_stock(reader, end, sector) == CAPACITY_TRANSFER_FAILURE &&
           ms_auto_read_sector(reader, end - 1, sector);
}

static int read_normal(SceUID file, uint32_t lba, uint8_t *out)
{
    SceOff position = (SceOff)lba * SECTOR_BYTES;
    if (sceIoLseek(file, position, PSP_SEEK_SET) != position ||
        sceIoRead(file, transfer, SECTOR_BYTES) != SECTOR_BYTES)
        return 0;
    memcpy(out, transfer, SECTOR_BYTES);
    return 1;
}

struct LowControls {
    uint8_t mbr[SECTOR_BYTES], sample[SECTOR_BYTES];
    uint32_t lba;
};

static int controls_match(struct StockReader *reader, const struct LowControls *controls)
{
    uint8_t sector[SECTOR_BYTES];
    return ms_auto_read_sector(reader, 0, sector) && !memcmp(sector, controls->mbr, sizeof(sector)) &&
           ms_auto_read_sector(reader, controls->lba, sector) && !memcmp(sector, controls->sample, sizeof(sector));
}

static int is_gpt(SceUID file, const uint8_t *mbr, uint32_t reported)
{
    if (mbr[510] == 0x55 && mbr[511] == 0xaa) {
        for (unsigned index = 0; index < 4; ++index) {
            if (mbr[450 + index * 16] == 0xee)
                return 1;
        }
    }
    uint8_t sector[SECTOR_BYTES];
    if (reported <= 1)
        return 0;
    if (!read_normal(file, 1, sector))
        return -1;
    return !memcmp(sector, "EFI PART", 8);
}

const char *ms_auto_profile(SceUID file, struct StockReader *reader, uint32_t *reported, int *block_count_hint)
{
    uint8_t attributes[MEDIA_ATTRIBUTE_BYTES] __attribute__((aligned(64)));
    struct MediaGeometry geometry;
    if (file < 0 || sceIoIoctl(file, CAPACITY_IOCTL, NULL, 0, reported, sizeof(*reported)) < 0 || !*reported)
        return "The adapter capacity query failed.";
    SceOff end = sceIoLseek(file, 0, PSP_SEEK_END);
    if (end != (SceOff)*reported * SECTOR_BYTES)
        return "The stock capacity and seek limit disagree.";
    int have_attributes = sceIoIoctl(file, ATTRIBUTES_IOCTL, NULL, 0, attributes, sizeof(attributes)) >= 0 &&
                          decode_media_geometry(&geometry, attributes);
    if (have_attributes && (geometry.unit_bytes != SECTOR_BYTES || geometry.bytes != (uint64_t)end))
        return "Memory Stick attributes disagree with the stock capacity.";
    if (block_count_hint)
        *block_count_hint = have_attributes && geometry.block_size == 4096;
    return stock_reader_init(reader, *reported);
}

const char *ms_auto_measure(SceUID file, struct MsAutoMedia *media)
{
    memset(media, 0, sizeof(*media));
    int block_count_hint;
    const char *error = ms_auto_profile(file, &media->reader, &media->reported, &block_count_hint);
    if (error)
        return error;

    struct LowControls controls;
    controls.lba = media->reported > SAMPLE_WINDOW ? SAMPLE_WINDOW : media->reported - 1;
    if (!read_normal(file, 0, controls.mbr) || !read_normal(file, controls.lba, controls.sample) ||
        !controls_match(&media->reader, &controls))
        return "The stock worker did not match the normal sector reads.";
    int gpt = is_gpt(file, controls.mbr, media->reported);
    if (gpt < 0)
        return "The current partition marks could not be read.";
    media->gpt = gpt;

    media->capacity = probe_auto_capacity(media->reported, block_count_hint, read_stock, &media->reader);
    if (!controls_match(&media->reader, &controls)) {
        media->capacity = (struct AutoCapacity){0};
        return "The card or low-sector samples changed during the scan.";
    }
    const struct AutoCapacity *capacity = &media->capacity;
    if (capacity->estimate.finding == CAPACITY_INCONCLUSIVE)
        return "The sector reads did not establish a capacity candidate.";
    if (!gpt && capacity->estimate.finding == CAPACITY_CANDIDATE &&
        capacity->estimate.sectors > media->reported) {
        uint8_t tail[SECTOR_BYTES];
        if (!ms_auto_read_sector(&media->reader, capacity->estimate.sectors - 1, tail))
            return "The final sector could not be checked for GPT.";
        media->gpt = !memcmp(tail, "EFI PART", 8);
    }
    media->layout = ms_format_layout(media->reported, capacity->estimate.sectors, &media->plan);
    if (media->layout == MS_FORMAT_EXTENDED && !media->gpt && capacity->estimate.finding == CAPACITY_CANDIDATE &&
        capacity->alias_checks && capacity->distinct_checks == capacity->alias_checks) {
        media->have_snapshot = ms_card_capture(&media->plan, NULL, media->reported, ms_auto_read_sector,
                                               &media->reader, &media->snapshot) &&
                               !memcmp(media->snapshot.mbr, controls.mbr, sizeof(controls.mbr)) &&
                               controls_match(&media->reader, &controls);
    }
    return NULL;
}
