/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fat32_probe.h"

struct Disk {
    struct Fat32Trial fixture;
    unsigned calls;
    enum { VALID, EXFAT, TOO_LARGE, SHORT_FAT, INVALID_CLUSTER, NO_FILE, READ_FAILURE } mode;
};

static int backing_read(void *context, uint32_t lba, unsigned char *sector)
{
    const struct Disk *disk = context;
    assert(lba == FAT32_TRIAL_FILE_SECTOR);
    if (disk->mode == READ_FAILURE)
        return 0;
    for (unsigned i = 0; i < SECTOR_BYTES; ++i)
        sector[i] = (unsigned char)i;
    return 1;
}

static int read_disk(void *context, uint32_t lba, unsigned char *sector)
{
    struct Disk *disk = context;
    assert(lba < FAT32_TRIAL_SECTORS);
    disk->calls++;
    if (!fat32_trial_read(&disk->fixture, lba, sector))
        return 0;
    if (lba == 0) {
        if (disk->mode == EXFAT)
            memcpy(sector + 3, "EXFAT   ", 8);
        if (disk->mode == TOO_LARGE)
            sector[35] = 0xff;
        if (disk->mode == SHORT_FAT) {
            memset(sector + 36, 0, 4);
            sector[36] = 1;
        }
    }
    if (!memcmp(sector, "PXPROBE BIN", 11)) {
        if (disk->mode == INVALID_CLUSTER) {
            memset(sector + 20, 0xff, 2);
            memset(sector + 26, 0xff, 2);
        }
        if (disk->mode == NO_FILE)
            sector[0] = 0;
    }
    return 1;
}

static void verify(struct Disk *disk, int mode, enum Fat32ProbeResult expected, unsigned backing_reads)
{
    unsigned char file[SECTOR_BYTES];
    disk->mode = mode;
    disk->calls = 0;
    disk->fixture.backing_reads = 0;
    assert(fat32_read_probe(read_disk, disk, FAT32_TRIAL_SECTORS, file) == expected);
    assert(disk->fixture.backing_reads == backing_reads);
    if (expected == FAT32_PROBE_MATCHED_FILE) {
        assert(disk->calls == 4);
        for (unsigned i = 0; i < SECTOR_BYTES; ++i)
            assert(file[i] == (unsigned char)i);
    }
}

int main(void)
{
    struct Disk disk = {.fixture = {backing_read, &disk, 0}};
    verify(&disk, VALID, FAT32_PROBE_MATCHED_FILE, 1);
    verify(&disk, EXFAT, FAT32_PROBE_NO_VOLUME, 0);
    verify(&disk, TOO_LARGE, FAT32_PROBE_BAD_LAYOUT, 0);
    verify(&disk, SHORT_FAT, FAT32_PROBE_BAD_LAYOUT, 0);
    verify(&disk, INVALID_CLUSTER, FAT32_PROBE_BAD_LAYOUT, 0);
    verify(&disk, NO_FILE, FAT32_PROBE_NO_FILE, 0);
    verify(&disk, READ_FAILURE, FAT32_PROBE_READ_ERROR, 1);
    unsigned char boot[SECTOR_BYTES];
    assert(read_disk(&disk, 0, boot));
    boot[29] = 8; /* Hidden sectors for a partition starting at LBA 2048. */
    assert(fat32_matches_extent(boot, 2048, FAT32_TRIAL_SECTORS));
    boot[65] ^= 1; /* A FAT32 boot status change does not change the extent. */
    assert(fat32_matches_extent(boot, 2048, FAT32_TRIAL_SECTORS));
    assert(!fat32_matches_extent(boot, 0, FAT32_TRIAL_SECTORS));
    assert(!fat32_matches_extent(boot, 2048, FAT32_TRIAL_SECTORS + 1));
    boot[510] = 0;
    assert(!fat32_matches_extent(boot, 2048, FAT32_TRIAL_SECTORS));
    puts("PASS: FAT32 file lookup, physical backing address and corrupt metadata refusal");
}
