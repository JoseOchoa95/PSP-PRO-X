/* GPL-3.0-or-later */
#include <string.h>

#include "partition_table.h"
#include "write_trial.h"

enum {
    FIRST_ENTRY = 446, ENTRY_BYTES = 16, MBR_ENTRIES = 4,
    ALIGNMENT = 2048, SECTORS_PER_GIB = 2097152
};

static void put_lba(unsigned char *dest, uint32_t lba)
{
    unsigned i;
    for (i = 0; i < 4; ++i)
        dest[i] = lba >> (8 * i);
}

static void put_chs(unsigned char *dest, uint32_t lba)
{
    uint32_t cylinder = lba / (255 * 63);
    if (cylinder > 1023) {
        dest[0] = 254;
        dest[1] = 255;
        dest[2] = 255;
        return;
    }
    dest[0] = lba / 63 % 255;
    dest[1] = ((cylinder >> 8) << 6) | (lba % 63 + 1);
    dest[2] = cylinder;
}

int build_trial_mbr(const unsigned char *original, unsigned char *result,
                    struct TrialPartition layout[TRIAL_PARTITIONS])
{
    const uint32_t full = TRIAL_REPORTED_BLOCKS / SECTORS_PER_GIB * SECTORS_PER_GIB;
    const unsigned char empty[ENTRY_BYTES * (MBR_ENTRIES - 1)] = {0};
    const unsigned char original_start[4] = {0, 8, 0, 0};
    uint32_t start = ALIGNMENT;
    unsigned i;
    if (primary_mbr_end(original) != TRIAL_PARTITION_END || original[FIRST_ENTRY + 4] != 7 ||
        memcmp(original + FIRST_ENTRY + 8, original_start, sizeof(original_start)) ||
        memcmp(original + FIRST_ENTRY + ENTRY_BYTES, empty, sizeof(empty)))
        return 0;
    if ((TRIAL_SECTORS - ALIGNMENT) / full != TRIAL_PARTITIONS - 1 ||
        (TRIAL_SECTORS - ALIGNMENT) % full == 0)
        return 0;

    memcpy(result, original, SECTOR_BYTES);
    memset(result + FIRST_ENTRY, 0, ENTRY_BYTES * MBR_ENTRIES);
    for (i = 0; i < TRIAL_PARTITIONS; ++i) {
        unsigned char *entry = result + FIRST_ENTRY + i * ENTRY_BYTES;
        uint32_t count = i < TRIAL_PARTITIONS - 1 ? full : TRIAL_SECTORS - start;
        layout[i] = (struct TrialPartition){start, count};
        entry[0] = i == 0 ? 0x80 : 0;
        entry[4] = 0x0c;
        put_chs(entry + 1, start);
        put_chs(entry + 5, start + count - 1);
        put_lba(entry + 8, start);
        put_lba(entry + 12, count);
        start += count;
    }
    return start == TRIAL_SECTORS;
}
