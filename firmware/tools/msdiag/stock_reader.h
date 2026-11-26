/* GPL-3.0-or-later */
#ifndef MSDIAG_STOCK_READER_H
#define MSDIAG_STOCK_READER_H

#include <stdint.h>

struct StockReader {
    uintptr_t text, data;
    uint32_t reported_blocks;
};

struct StockReadTrace {
    unsigned calls;
    int first_result, last_result;
};

/* A non-NULL return explains why the diagnostic stopped before the transfer. */
const char *stock_reader_init(struct StockReader *reader, uint32_t reported_blocks);
const char *stock_read_sector(const struct StockReader *reader, uint32_t lba,
                              void *buffer, int *result, struct StockReadTrace *trace);

#endif
