/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "partition_table.h"
#include "write_trial.h"

static void put32(unsigned char *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = value >> (8 * i);
}

int main(int argc, char **argv)
{
    unsigned char original[SECTOR_BYTES] = {0}, mbr[SECTOR_BYTES];
    const unsigned char empty[16] = {0};
    struct TrialPartition layout[TRIAL_PARTITIONS];
    original[0] = 0xfa;
    original[440] = 0x5b;
    original[450] = 7;
    put32(original + 454, 2048);
    put32(original + 458, TRIAL_PARTITION_END - 2048);
    original[510] = 0x55;
    original[511] = 0xaa;

    assert(build_trial_mbr(original, mbr, layout));
    assert(layout[0].start == 2048 && layout[0].count == 230686720);
    assert(layout[1].start == 230688768 && layout[1].count == 230686720);
    assert(layout[2].start == 461375488 && layout[2].count == 39319552);
    assert(mbr[446] == 0x80 && mbr[462] == 0 && mbr[478] == 0);
    assert(mbr[450] == 0x0c && mbr[466] == 0x0c && mbr[482] == 0x0c);
    assert(!memcmp(original, mbr, 446) && !memcmp(mbr + 494, empty, sizeof(empty)));
    assert(primary_mbr_end(mbr) == TRIAL_SECTORS);
    if (argc == 2) {
        FILE *output = fopen(argv[1], "wb");
        assert(output && fwrite(mbr, 1, sizeof(mbr), output) == sizeof(mbr));
        assert(fclose(output) == 0);
    }
    original[466] = 0x0f;
    assert(!build_trial_mbr(original, mbr, layout));
    original[466] = 0;
    original[454] = 1;
    put32(original + 458, TRIAL_PARTITION_END - 2049);
    assert(!build_trial_mbr(original, mbr, layout));
    puts("PASS: three bounded primary partitions, preserved boot fields and invalid layout refusal");
    return 0;
}
