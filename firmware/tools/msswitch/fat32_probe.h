/* GPL-3.0-or-later */
#ifndef MSSWITCH_FAT32_PROBE_H
#define MSSWITCH_FAT32_PROBE_H

#include "probe.h"

typedef int (*ReadFatSector)(void *context, uint32_t lba, unsigned char sector[SECTOR_BYTES]);

int fat32_matches_extent(const unsigned char boot[SECTOR_BYTES], uint32_t start, uint32_t sectors);

enum Fat32ProbeResult {
    FAT32_PROBE_MATCHED_FILE,
    FAT32_PROBE_NO_VOLUME,
    FAT32_PROBE_NO_FILE,
    FAT32_PROBE_BAD_LAYOUT,
    FAT32_PROBE_READ_ERROR,
};

/* Look for the 512-byte PXPROBE.BIN in the root directory of a bounded volume. */
enum Fat32ProbeResult fat32_read_probe(ReadFatSector read, void *context, uint32_t sectors,
                                     unsigned char file[SECTOR_BYTES]);

enum { FAT32_TRIAL_SECTORS = 39319552, FAT32_TRIAL_FILE_SECTOR = 39318528 };

struct Fat32Trial {
    ReadFatSector backing_read;
    void *backing_context;
    unsigned backing_reads;
};

/* Synthetic FAT32 metadata with the file's first sector supplied by the real volume. */
int fat32_trial_read(void *context, uint32_t lba, unsigned char sector[SECTOR_BYTES]);

#endif
