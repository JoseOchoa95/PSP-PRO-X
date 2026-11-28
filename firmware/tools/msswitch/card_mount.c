/* GPL-3.0-or-later */
#include <pspiofilemgr_kernel.h>
#include <string.h>

#include "card_mount.h"
#include "format_output.h"
#include "fat32_probe.h"
#include "sony_mkdir_ram.h"
#include "stock_reader.h"

static const char backing[] = "msstor0p1:";

void ms_mount_report(void)
{
    struct SonyFatDetection detection;
    if (!sony_fat_stock_detection(&detection)) {
        format_trace("Sony FAT context unavailable.\n");
        return;
    }
    format_trace("FAT state %08x  mounts %u  handle %08x\n",
                         (unsigned)detection.state, (unsigned)detection.mounts, (unsigned)detection.handle);
    format_trace("Media query %08x  present %u\n", (unsigned)detection.media_result,
                         (unsigned)detection.present);
    format_trace("Event %08x  query %08x  name %s\n", (unsigned)detection.event,
                         (unsigned)detection.event_result, detection.event_name_matches ? "MATCH" : "unconfirmed");
    format_trace("Waiters %d  pending %08x  active %u\n", detection.waiters,
                         (unsigned)detection.pending, (unsigned)detection.activity);
}

static int verify_first_boot(const struct MsFormatVolume *first, uint32_t volume_id)
{
    uint8_t sector[FAT32_SECTOR_BYTES] __attribute__((aligned(64)));
    const uint8_t serial[] = {volume_id, volume_id >> 8, volume_id >> 16, volume_id >> 24};
    SceUID raw = sceIoOpen(backing, PSP_O_RDONLY, 0);
    format_trace("P1 native open: 0x%08x\n", (unsigned)raw);
    if (raw < 0)
        return 0;
    int bytes = sceIoRead(raw, sector, sizeof(sector));
    int close_result = sceIoClose(raw);
    if (bytes != (int)sizeof(sector) || close_result || memcmp(sector + 67, serial, sizeof(serial)) ||
        !fat32_matches_extent(sector, first->start, first->sectors)) {
        format_trace("P1 boot: FAIL   Read %d\n", bytes);
        format_trace("P1 close: 0x%08x\n", (unsigned)close_result);
        return 0;
    }
    format_trace("P1 boot: MATCH\n");
    return 1;
}

int ms_mount_first(const struct StockReader *reader, const struct MsFormatVolume *first, uint32_t volume_id)
{
    int refresh = stock_format_refresh_media(reader);
    format_trace("Sony refresh: 0x%08x\n", (unsigned)refresh);
    if (refresh < 0 || !verify_first_boot(first, volume_id))
        return 0;

    int unassign = sceIoUnassign("ms0:");
    format_trace("ms0 unassign: 0x%08x\n", (unsigned)unassign);
    if (unassign < 0 && (uint32_t)unassign != 0x80020321)
        return 0;

    uint32_t state, mounts;
    if (!sony_fat_stock_state(&state, &mounts))
        return 0;
    if (state & SONY_FAT_WHOLE_DEVICE) {
        enum SonyFatReprobe result = sony_fat_stock_reprobe();
        format_trace("Sony FAT reopen: %u\n", (unsigned)result);
        if (result != SONY_FAT_REPROBED) {
            ms_mount_report();
            return 0;
        }
        if (!verify_first_boot(first, volume_id) ||
            !sony_fat_stock_state(&state, &mounts) || (state & SONY_FAT_WHOLE_DEVICE))
            return 0;
    }
    if (!(state & SONY_FAT_MOUNTED) && !mounts) {
        int assigned = sceIoAssign("ms0:", backing, "fatms0:", IOASSIGN_RDWR, NULL, 0);
        format_trace("ms0 assign: 0x%08x\n", (unsigned)assigned);
        if (assigned < 0)
            return 0;
    }
    SceIoStat game, marker;
    int dir_result = sceIoGetstat("ms0:/PSP/GAME", &game);
    int file_result = sceIoGetstat("ms0:/MEMSTICK.IND", &marker);
    format_trace("ms0 GAME: 0x%08x   IND: 0x%08x\n", (unsigned)dir_result, (unsigned)file_result);
    return dir_result >= 0 && FIO_S_ISDIR(game.st_mode) && file_result >= 0 &&
           FIO_S_ISREG(marker.st_mode) && marker.st_size == 0;
}
