/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fat32_dirs.h"
#include "job.h"

enum Failure {
    OK, UNVERIFIED, SWAP, DENIED, BAD_TREE, WRITE_ERROR, READBACK, DROPPED_WRITE, CLEANUP, MOUNT,
    BATCH_REFUSED, LATE_READBACK,
};

enum { FIRST_BOOT = 2048, SECOND_BOOT = 230688768, CORRUPT_METADATA = FIRST_BOOT + 41 };

static struct TestDisk {
    enum Failure mode;
    struct MsFormatPlan plan;
    int locked, first_boot_published;
    unsigned writes, stages, mounts, entries, reports, sector_reads, batch_reads;
    struct { uint32_t lba; uint8_t data[512]; } sectors[160];
} disk;

static void put32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        out[i] = (uint8_t)(value >> (i * 8));
}

static void entry(uint8_t *out, const char *name, uint32_t cluster)
{
    memset(out, ' ', 11);
    memcpy(out, name, strlen(name));
    out[11] = 0x10;
    out[26] = (uint8_t)cluster;
}

static int seed_initial_contents(void *context, struct Fat32Ram *image, const struct MsFormatVolume *volume,
                                 unsigned index)
{
    static const char *const names[] = {
        "PSP", "GAME", "SAVEDATA", "COMMON", "SYSTEM", "MP_ROOT", "100MNV01", "101ANV01",
        "MUSIC", "PICTURE", "VIDEO", "THEME", "RSSCH", "IMPORT", "SEPLUG~1", "ISO", "VIDEO",
    };
    static const unsigned parents[] = {0, 1, 1, 1, 1, 0, 6, 6, 0, 0, 0, 1, 1, 13, 0, 0, 16};
    static const uint8_t seplugins[32] = {
        0x41, 'S', 0, 'E', 0, 'P', 0, 'L', 0, 'U', 0, 0x0f, 0, 0xa3,
        'G', 0, 'I', 0, 'N', 0, 'S', 0, 0, 0, 0xff, 0xff, 0, 0, 0xff, 0xff, 0xff, 0xff,
    };
    struct TestDisk *test = context;
    assert(test->locked && test->stages++ == index);
    image->start = volume->start;
    image->length = volume->sectors;
    assert(fat32_initialize(volume->start, volume->sectors, 0x50580000 + index,
                            fat32_ram_write, image) == FAT32_INIT_OK);
    if (test->mode == BAD_TREE)
        return 1;
    struct Fat32Layout fat;
    uint8_t data[512];
    assert(fat32_plan(volume->sectors, &fat));
    assert(fat32_ram_read(image, 32, data));
    for (unsigned i = 0; i < PSP_INITIAL_DIRECTORY_COUNT; ++i)
        put32(data + (i + 3) * 4, 0x0fffffff);
    assert(fat32_ram_write(image, 32, data) && fat32_ram_write(image, 32 + fat.fat_sectors, data));
    for (unsigned parent = 0; parent <= PSP_INITIAL_DIRECTORY_COUNT; ++parent) {
        memset(data, 0, sizeof(data));
        unsigned offset = 0;
        if (parent) {
            entry(data, ".", parent + 2);
            entry(data + 32, "..", parents[parent - 1] ? parents[parent - 1] + 2 : 0);
            offset = 64;
        }
        for (unsigned i = 0; i < PSP_INITIAL_DIRECTORY_COUNT; ++i) {
            if (parents[i] != parent)
                continue;
            if (i == 14) {
                memcpy(data + offset, seplugins, sizeof(seplugins));
                offset += 32;
            }
            entry(data + offset, names[i], i + 3);
            offset += 32;
        }
        assert(fat32_ram_write(image, fat.root_sector + parent * fat.sectors_per_cluster, data));
    }
    return fat32_ram_add_indicators(image);
}

static int inspect(void *context, struct MsFormatMedia *media)
{
    const struct TestDisk *test = context;
    *media = (struct MsFormatMedia){232259584, 1048576000, {3}};
    if (test->mode == SWAP && test->locked)
        media->identity[0] = 4;
    return test->mode != UNVERIFIED;
}

static enum MsFormatAccess exclusive(void *context, MsFormatLockedOperation run, void *job)
{
    struct TestDisk *test = context;
    assert(!test->locked);
    if (test->mode == DENIED)
        return MS_FORMAT_ACCESS_DENIED;
    test->locked = 1;
    run(job);
    test->locked = 0;
    return test->mode == CLEANUP ? MS_FORMAT_ACCESS_CLEANUP_FAILED : MS_FORMAT_ACCESS_OK;
}

static int allowed(const struct TestDisk *test, uint32_t lba)
{
    if (lba == 0)
        return 1;
    for (unsigned i = 0; i < test->plan.volume_count; ++i) {
        struct MsFormatVolume volume;
        struct Fat32Layout fat;
        assert(ms_format_volume(&test->plan, i, &volume));
        assert(fat32_plan(volume.sectors, &fat));
        if (lba == volume.ebr || (lba >= volume.start &&
            lba - volume.start < fat.root_sector + (PSP_INITIAL_DIRECTORY_COUNT + 1) * fat.sectors_per_cluster))
            return 1;
    }
    return 0;
}

static void read_fixture(const struct TestDisk *test, uint32_t lba, uint8_t *out)
{
    assert(test->locked && allowed(test, lba));
    memset(out, 0, 512);
    if (test->mode == DROPPED_WRITE && lba == SECOND_BOOT)
        memset(out, 0xa5, 512);
    for (unsigned i = 0; i < test->entries; ++i) {
        if (test->sectors[i].lba == lba) {
            memcpy(out, test->sectors[i].data, 512);
            if (test->mode == READBACK && lba == 2048 && out[510] == 0x55)
                out[10] ^= 1;
            break;
        }
    }
    if (test->mode == LATE_READBACK && test->first_boot_published && lba == CORRUPT_METADATA)
        out[0] ^= 1;
}

static int read_sector(void *context, uint32_t lba, uint8_t *out)
{
    struct TestDisk *test = context;
    ++test->sector_reads;
    read_fixture(test, lba, out);
    return 1;
}

static int read_batch(void *context, uint32_t lba, unsigned count, uint8_t *out)
{
    struct TestDisk *test = context;
    assert(count > 1 && count <= MS_FORMAT_READ_BATCH_SECTORS && !test->sectors[0].data[510]);
    ++test->batch_reads;
    if (test->mode == BATCH_REFUSED)
        return 0;
    for (unsigned i = 0; i < count; ++i)
        read_fixture(test, lba + i, out + i * FAT32_SECTOR_BYTES);
    return 1;
}

static int write_sector(void *context, uint32_t lba, const uint8_t *data)
{
    struct TestDisk *test = context;
    assert(test->locked && allowed(test, lba));
    if (++test->writes == 6 && test->mode == WRITE_ERROR)
        return 0;
    if (test->mode == DROPPED_WRITE && lba == SECOND_BOOT)
        return 1;
    unsigned i = 0;
    while (i < test->entries && test->sectors[i].lba != lba)
        ++i;
    if (i == test->entries) {
        assert(test->entries < sizeof(test->sectors) / sizeof(test->sectors[0]));
        ++test->entries;
        test->sectors[i].lba = lba;
    }
    memcpy(test->sectors[i].data, data, 512);
    if (lba == FIRST_BOOT && data[510] == 0x55)
        test->first_boot_published = 1;
    return 1;
}

static int mount_first(void *context, const struct MsFormatMedia *media, const struct MsFormatVolume *volume)
{
    struct TestDisk *test = context;
    assert(!test->locked && media->identity[0] == 3 && volume->start == 2048 && !volume->ebr);
    assert(test->sectors[0].lba == 0 && test->sectors[0].data[510] == 0x55 && test->stages == 5);
    assert(test->sectors[0].data[440] == 3 && test->sectors[0].data[446] == 0x80);
    ++test->mounts;
    return test->mode != MOUNT;
}

static void verify_report(void *context, unsigned index, uint32_t writes)
{
    struct TestDisk *test = context;
    assert(test->locked && index == test->reports && writes >= 4 && writes < test->writes);
    ++test->reports;
}

static const struct MsFormatBackend backend = {inspect, exclusive, read_sector, write_sector,
                                               seed_initial_contents, mount_first, verify_report, read_batch};
static struct Fat32Ram scratch;

static void run(enum Failure injection, enum MsFormatFailure expected, unsigned verified, int wrote)
{
    memset(&disk, 0, sizeof(disk));
    disk.mode = injection;
    struct MsFormatPreview preview;
    enum MsFormatLayoutResult status = ms_format_preview(&backend, &disk, &preview);
    assert(status == (injection == UNVERIFIED ? MS_FORMAT_INVALID : MS_FORMAT_EXTENDED));
    disk.plan = preview.plan;
    struct MsFormatResult result = ms_format_execute(&backend, &disk, &preview, &scratch);
    assert(result.failure == expected && result.verified_volumes == verified);
    assert(disk.reports == verified);
    assert(result.writes == disk.writes && !!disk.writes == wrote && !disk.locked);
    if (injection == DROPPED_WRITE) {
        assert(result.sector_failure == MS_SECTOR_MISMATCH && result.failure_lba == SECOND_BOOT);
        assert(disk.stages == 2 && result.writes < 32);
        assert(disk.sectors[0].data[510] == 0);
    }
    if (injection == LATE_READBACK) {
        assert(result.sector_failure == MS_SECTOR_MISMATCH && result.failure_lba == CORRUPT_METADATA);
        assert(disk.batch_reads && disk.sectors[0].data[510] == 0);
    }
    assert(disk.mounts == (unsigned)(injection == OK || injection == MOUNT || injection == BATCH_REFUSED));
    assert(ms_format_execute(&backend, &disk, &preview, &scratch).failure == MS_FORMAT_NO_PREVIEW);
}

int main(void)
{
    struct MsFormatPreview preview;
    assert(ms_format_preview(&backend, &disk, &preview) == MS_FORMAT_EXTENDED);
    ms_format_cancel(&preview);
    assert(ms_format_execute(&backend, &disk, &preview, &scratch).failure == MS_FORMAT_NO_PREVIEW);
    assert(!disk.writes);
    run(UNVERIFIED, MS_FORMAT_NO_PREVIEW, 0, 0);
    run(SWAP, MS_FORMAT_MEDIA_CHANGED, 0, 0);
    run(DENIED, MS_FORMAT_ACCESS, 0, 0);
    run(BAD_TREE, MS_FORMAT_INITIAL_CONTENTS, 0, 0);
    run(WRITE_ERROR, MS_FORMAT_VOLUME_IO, 0, 1);
    run(READBACK, MS_FORMAT_VOLUME_IO, 0, 1);
    run(DROPPED_WRITE, MS_FORMAT_VOLUME_IO, 1, 1);
    run(CLEANUP, MS_FORMAT_RELEASE_FAILED, 5, 1);
    run(MOUNT, MS_FORMAT_FIRST_MOUNT, 5, 1);
    run(LATE_READBACK, MS_FORMAT_VOLUME_IO, 0, 1);
    run(BATCH_REFUSED, MS_FORMAT_COMPLETE, 5, 1);
    unsigned single_reads = disk.sector_reads;
    unsigned writes = disk.writes;
    run(OK, MS_FORMAT_COMPLETE, 5, 1);
    assert(disk.batch_reads && disk.writes == writes && disk.sector_reads + disk.batch_reads < single_reads / 4);
    puts("PASS: preview, cancellation, wrong card and bounds, volume and table readback, final mount and failures");
    return 0;
}
