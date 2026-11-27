/* GPL-3.0-or-later */
#include <string.h>

#include "../msdiag/probe.h"
#include "card_snapshot.h"
#include "fat32_dirs.h"

static int safe_witness(const struct MsFormatPlan *plan, uint32_t lba)
{
    if (!lba || lba >= plan->usable_sectors)
        return 0;
    for (unsigned i = 0; i < plan->volume_count; ++i) {
        struct MsFormatVolume volume;
        struct Fat32Layout fat;
        if (!ms_format_volume(plan, i, &volume) || !fat32_plan(volume.sectors, &fat))
            return 0;
        uint32_t metadata = fat.root_sector + (PSP_INITIAL_DIRECTORY_COUNT + 1) * fat.sectors_per_cluster;
        if (lba == volume.ebr || (lba >= volume.start && lba - volume.start < metadata))
            return 0;
    }
    return 1;
}

static int nonuniform(const uint8_t sector[FAT32_SECTOR_BYTES])
{
    for (unsigned i = 1; i < FAT32_SECTOR_BYTES; ++i) {
        if (sector[i] != sector[0])
            return 1;
    }
    return 0;
}

int ms_card_matches(const struct MsCardSnapshot *snapshot, int include_mbr,
                    MsReadSector read, void *device)
{
    uint8_t sector[FAT32_SECTOR_BYTES];
    if (!snapshot || !read || !snapshot->witnesses[0].lba || !snapshot->witnesses[1].lba)
        return 0;
    if (include_mbr && (!read(device, 0, sector) || memcmp(sector, snapshot->mbr, sizeof(sector))))
        return 0;
    for (unsigned i = 0; i < MS_FORMAT_WITNESSES; ++i) {
        if (!read(device, snapshot->witnesses[i].lba, sector) ||
            memcmp(sector, snapshot->witnesses[i].sector, sizeof(sector)))
            return 0;
    }
    return 1;
}

int ms_card_capture(const struct MsFormatPlan *plan, const struct MsGptLayout *gpt, uint32_t reported, MsReadSector read,
                    void *device, struct MsCardSnapshot *snapshot)
{
    if (!snapshot)
        return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!plan || !read || !plan->volume_count || !reported || reported >= plan->usable_sectors ||
        !read(device, 0, snapshot->mbr))
        return 0;

    uint32_t length = plan->usable_sectors;
    const uint32_t bases[] = {BINARY_CUTOFF, reported, length - 1, length / 2,
                              length / 4, length / 4 * 3, 1};
    const int offsets[] = {0, 1, 64, 1024, 2048, -1, -1023, -2047};
    uint8_t sector[FAT32_SECTOR_BYTES];
    unsigned found = 0;
    for (unsigned i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        for (unsigned j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j) {
            int64_t candidate = (int64_t)bases[i] + offsets[j];
            if (candidate <= 0 || candidate >= length || (!found && candidate < reported) ||
                ms_gpt_contains(gpt, (uint32_t)candidate) ||
                !safe_witness(plan, (uint32_t)candidate) || (uint32_t)candidate == snapshot->witnesses[0].lba)
                continue;
            if (!read(device, (uint32_t)candidate, sector))
                goto failed;
            if (!nonuniform(sector) || (found && !memcmp(sector, snapshot->witnesses[0].sector, sizeof(sector))))
                continue;
            snapshot->witnesses[found].lba = (uint32_t)candidate;
            memcpy(snapshot->witnesses[found].sector, sector, sizeof(sector));
            if (++found == MS_FORMAT_WITNESSES) {
                if (ms_card_matches(snapshot, 1, read, device))
                    return 1;
                goto failed;
            }
        }
    }
failed:
    memset(snapshot, 0, sizeof(*snapshot));
    return 0;
}
