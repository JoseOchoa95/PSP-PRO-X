/* GPL-3.0-or-later */
#ifndef MSSWITCH_PARTITION_VIEW_H
#define MSSWITCH_PARTITION_VIEW_H

#include <stddef.h>
#include <stdint.h>

enum { MBR_PARTITIONS = 4 };

struct PartitionView {
    uint32_t start, sectors;
};

size_t read_primary_views(const unsigned char *mbr, uint32_t medium_sectors,
                          struct PartitionView views[MBR_PARTITIONS]);
int view_sector(const struct PartitionView *view, uint64_t local_lba, uint32_t *absolute_lba);
int64_t view_seek(const struct PartitionView *view, int64_t position, int64_t offset, int whence);

#endif
