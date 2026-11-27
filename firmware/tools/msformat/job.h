/* GPL-3.0-or-later */
#ifndef MSFORMAT_JOB_H
#define MSFORMAT_JOB_H

#include "fat32_ram.h"
#include "layout.h"

struct MsFormatMedia {
    uint64_t reported_sectors, verified_sectors;
    uint8_t identity[20];
};

struct MsFormatPreview {
    struct MsFormatMedia media;
    struct MsFormatPlan plan;
};

enum MsFormatAccess {
    MS_FORMAT_ACCESS_DENIED,
    MS_FORMAT_ACCESS_OK,
    MS_FORMAT_ACCESS_CLEANUP_FAILED,
};

typedef void (*MsFormatLockedOperation)(void *context);

enum { MS_FORMAT_READ_BATCH_SECTORS = 8 };

struct MsFormatBackend {
    /* A successful inspection attests to the usable extent and insertion identity. */
    int (*inspect)(void *device, struct MsFormatMedia *media);
    /* Invoke the operation once, synchronously, while the target is exclusive.
       Sector operations must reject a change of insertion while it runs. */
    enum MsFormatAccess (*exclusive)(void *device, MsFormatLockedOperation operation, void *context);
    int (*read)(void *device, uint32_t lba, uint8_t sector[FAT32_SECTOR_BYTES]);
    Fat32WriteSector write;
    /* Mount, create initial contents and unmount in a private RAM context. */
    int (*stage)(void *device, struct Fat32Ram *image, const struct MsFormatVolume *volume,
                 unsigned index);
    /* Restore the normal partition 1 mount and verify its identity and root. */
    int (*mount_first)(void *device, const struct MsFormatMedia *media,
                       const struct MsFormatVolume *volume);
    void (*volume_verified)(void *device, unsigned index, uint32_t writes);
    /* Optional contiguous metadata read. A failure falls back to sector reads. */
    int (*read_batch)(void *device, uint32_t lba, unsigned count, uint8_t *sectors);
};

enum MsFormatFailure {
    MS_FORMAT_COMPLETE,
    MS_FORMAT_NO_PREVIEW,
    MS_FORMAT_ACCESS,
    MS_FORMAT_MEDIA_CHANGED,
    MS_FORMAT_INITIAL_CONTENTS,
    MS_FORMAT_VOLUME_IO,
    MS_FORMAT_TABLE_IO,
    MS_FORMAT_RELEASE_FAILED,
    MS_FORMAT_FIRST_MOUNT,
};

enum MsFormatSectorFailure {
    MS_SECTOR_OK,
    MS_SECTOR_READ_FAILED,
    MS_SECTOR_WRITE_FAILED,
    MS_SECTOR_MISMATCH,
};

struct MsFormatResult {
    enum MsFormatFailure failure;
    unsigned verified_volumes;
    uint32_t writes;
    enum MsFormatSectorFailure sector_failure;
    uint32_t failure_lba;
};

enum MsFormatLayoutResult ms_format_preview(const struct MsFormatBackend *backend, void *device,
                                            struct MsFormatPreview *preview);
void ms_format_cancel(struct MsFormatPreview *preview);
/* The caller confirms the preview before invoking this function. Execution consumes it. */
struct MsFormatResult ms_format_execute(const struct MsFormatBackend *backend, void *device,
                                        struct MsFormatPreview *preview, struct Fat32Ram *scratch);

#endif
