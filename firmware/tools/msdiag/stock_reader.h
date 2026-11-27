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

#ifdef MSDIAG_ATTRIBUTE_PROBE
const char *stock_read_attribute_page(const struct StockReader *reader, uint32_t page, void *buffer,
                                     int *result);
#endif

#ifdef MSDIAG_WRITE_TRIAL
struct StockWriteState {
    int observed;
    uint32_t media_type, open_handles, write_enabled, write_protected;
};

const char *stock_write_trial_check(const struct StockReader *reader,
                                    struct StockWriteState *observed);
const char *stock_write_trial(const struct StockReader *reader, const void *original,
                              void *replacement, const void *mbr_digest,
                              int *result, int *dispatched);
#endif

#ifdef MSFORMAT_TRIAL
struct WriteTrialReference;
#ifdef MSDIRS_CARD_TRIAL
struct Fat32Ram;
#endif
struct StockFormatTrial {
    const struct StockReader *reader;
    const struct WriteTrialReference *reference;
    const unsigned char *confirmed_boot;
    unsigned writes, verified;
    int worker_result, command_result;
    const char *error;
#ifdef MSDIRS_CARD_TRIAL
    struct Fat32Ram *directory_image;
#endif
};

int stock_format_partition_trial(struct StockFormatTrial *trial);
#endif

#ifdef MSMULTI_CARD_TRIAL
struct StockFormatAccess;
typedef void (*StockFormatOperation)(struct StockFormatAccess *access, void *context);
int stock_format_transaction(const struct StockReader *reader, StockFormatOperation operation,
                             void *context, uint32_t sector_limit);
int stock_format_locked_read(struct StockFormatAccess *access, uint32_t lba, void *buffer);
#ifdef MSAUTO_FORMAT_TRIAL
enum { STOCK_FORMAT_READ_BATCH_SECTORS = 8 };
int stock_format_locked_read_batch(struct StockFormatAccess *access, uint32_t lba, unsigned count, void *buffer);
#endif
int stock_format_locked_write(struct StockFormatAccess *access, uint32_t lba, const void *buffer);
int stock_format_locked_write_status(struct StockFormatAccess *access, uint32_t lba,
                                     const void *buffer, int *command);
int stock_format_refresh_media(const struct StockReader *reader);
#endif

#endif
