/* GPL-3.0-or-later */
#include <string.h>

#include "../msdiag/probe.h"
#include "fat32_dirs.h"
#include "write_probe.h"

uint32_t ms_probe_high_metadata(const struct MsFormatPlan *plan, uint32_t reported)
{
    if (!reported || reported >= plan->usable_sectors)
        return 0;
    for (unsigned i = 0; i < plan->volume_count; ++i) {
        struct MsFormatVolume volume;
        struct Fat32Layout fat;
        if (!ms_format_volume(plan, i, &volume) || !fat32_plan(volume.sectors, &fat))
            return 0;
        uint32_t lba = volume.start > reported ? volume.start : reported;
        uint32_t metadata = fat.root_sector + (PSP_INITIAL_DIRECTORY_COUNT + 1) * fat.sectors_per_cluster;
        /* Skip the first address of each sampled wrap period. */
        while (lba - volume.start < metadata &&
               (!(lba % reported) || !(lba % MS_PROBE_FOUR_GIB_SECTORS) ||
                !(lba % DECIMAL_CUTOFF) || !(lba % BINARY_CUTOFF)))
            ++lba;
        if (lba - volume.start < metadata)
            return lba;
    }
    return 0;
}

struct MsWriteProbeResult ms_probe_sector(uint32_t lba, MsReadSector read, Fat32WriteSector write,
                                         void *device)
{
    struct MsWriteProbeResult result = {0};
    uint8_t original[FAT32_SECTOR_BYTES], pattern[FAT32_SECTOR_BYTES], actual[FAT32_SECTOR_BYTES];
    if (!read(device, lba, original))
        return result;
    for (unsigned i = 0; i < sizeof(pattern); ++i)
        pattern[i] = original[i] ^ 0xa5;
    result.attempted = 1;
    result.accepted = write(device, lba, pattern);
    if (read(device, lba, actual)) {
        if (!memcmp(actual, original, sizeof(original))) {
            result.readback = MS_PROBE_ORIGINAL;
            result.restored = 1;
            return result;
        }
        result.readback = !memcmp(actual, pattern, sizeof(pattern)) ? MS_PROBE_PATTERN : MS_PROBE_DIFFERENT;
    }
    /* A rejected return can still follow a transfer. Judge restoration by
       reading the original bytes, including when the device reports failure. */
    write(device, lba, original);
    result.restored = read(device, lba, actual) && !memcmp(actual, original, sizeof(original));
    return result;
}
