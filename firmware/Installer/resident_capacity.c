/* Measure and budget flash0 before the manual resident installation. GPL-3.0-or-later. */
#ifdef PROX_CAPACITY_TEST
#include "resident_loader_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>
#include "kubridge.h"
#endif
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>
#include "stage.h"
#include "prox_flash_capacity.h"
#include "prox_resident_image.h"
#include "flash_capacity_result.h"
#include "flash_capacity_prx.h"

static int capacity_error(const char *operation, int error)
{
    pspDebugScreenPrintf("flash0 check: %s failed (0x%08x).\n", operation, (unsigned int)error);
    return error < 0 ? error : -1;
}

static int measure_mounted_flash(uint32_t *cluster_bytes, uint32_t *free_bytes)
{
    /* Sony 6.61 flashfat's read-only volume information command. */
    enum { FLASH_FAT_VOLUME_INFO = 0x0003d814 };
    struct {
        uint32_t cluster_bytes, used_clusters, free_clusters, used_root_entries, free_root_entries;
    } volume = {0};
    int result = sceIoDevctl("flash0:", FLASH_FAT_VOLUME_INFO, NULL, 0, &volume, sizeof(volume));
    if (result < 0)
        return result;

    uint32_t cluster = volume.cluster_bytes;
    uint64_t total_clusters = (uint64_t)volume.used_clusters + volume.free_clusters;
    if (result || cluster < PROX_FLASH_SECTOR_BYTES || cluster > 65536u ||
        (cluster & (cluster - 1)) || !total_clusters || total_clusters >= 65525 ||
        total_clusters * cluster > PROX_FLASH_MAX_VOLUME_BYTES)
        return -1;

    *cluster_bytes = cluster;
    *free_bytes = volume.free_clusters * cluster;
    return 0;
}

static int read_flash_sectors(void *context, uint32_t offset, void *buffer, unsigned int bytes)
{
    SceUID fd = *(const SceUID *)context;
    SceOff position = sceIoLseek(fd, offset, PSP_SEEK_SET);
    if (position != (SceOff)offset) {
        int error = position < 0 ? (int)position : -1;
        pspDebugScreenPrintf("flash0 check: seek at 0x%08x failed (0x%08x).\n",
                             (unsigned int)offset, (unsigned int)error);
        return error;
    }
    int count = sceIoRead(fd, buffer, bytes);
    if (count == (int)bytes)
        return 0;
    int error = count < 0 ? count : -1;
    pspDebugScreenPrintf("flash0 check: read at 0x%08x failed (0x%08x).\n",
                         (unsigned int)offset, (unsigned int)error);
    return error;
}

static uint32_t flash_le32(const unsigned char *bytes)
{
    return bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static uint32_t flash_le16(const unsigned char *bytes)
{
    return bytes[0] | (uint32_t)bytes[1] << 8;
}

static uint32_t boot_sector_count(const unsigned char *boot)
{
    uint32_t total16 = flash_le16(boot + 19);
    return total16 ? total16 : flash_le32(boot + 32);
}

static int confirm_sony_sector_boundary(SceUID fd, uint32_t length, void *sector, SceOff invalid_offset)
{
    if (sceKernelDevkitVersion() != 0x06060110u || length < 2 * PROX_FLASH_SECTOR_BYTES)
        return 0;

    /* The stock 6.61 lflash driver checks ceil(offset / 512) < sectors.
     * Its read handler applies the same check to the endpoint of the read.
     * Match that boundary and verify the preceding read advances to it. */
    uint32_t last = length - PROX_FLASH_SECTOR_BYTES;
    SceOff cursor = sceIoLseek(fd, last, PSP_SEEK_SET);
    SceOff after = sceIoLseek(fd, last + 1, PSP_SEEK_SET);
    if (cursor != (SceOff)last || after != invalid_offset) {
        pspDebugScreenPrintf("flash0 check: last sector %08x, next byte %08x.\n",
                             (unsigned int)cursor, (unsigned int)after);
        return 0;
    }
    if (read_flash_sectors(&fd, last - PROX_FLASH_SECTOR_BYTES, sector, PROX_FLASH_SECTOR_BYTES) < 0 ||
        sceIoLseek(fd, 0, PSP_SEEK_CUR) != (SceOff)last)
        return 0;
    pspDebugScreenPrintf("flash0 check: verified Sony sector boundary.\n");
    return 1;
}

static int confirm_flash_seek_boundary(SceUID fd, uint32_t length, int tail_error, void *sector)
{
    /* Accept only a boundary set by the driver: the declared EOF must be a
     * valid cursor, and the next sector must be refused or clamped to EOF. */
    const SceOff invalid_offset = (int32_t)0x80010016u;
    SceOff eof = sceIoLseek(fd, length, PSP_SEEK_SET);
    SceOff beyond = sceIoLseek(fd, length + PROX_FLASH_SECTOR_BYTES, PSP_SEEK_SET);
    if (eof == (SceOff)length && (beyond == eof || beyond == invalid_offset) &&
        sceIoLseek(fd, length, PSP_SEEK_SET) == eof) {
        pspDebugScreenPrintf("flash0 check: verified seek boundary.\n");
        return 1;
    }

    const int access_denied = (int32_t)0x8001000du;
    if (tail_error == access_denied && eof == invalid_offset && beyond == invalid_offset &&
        confirm_sony_sector_boundary(fd, length, sector, invalid_offset))
        return 1;

    pspDebugScreenPrintf("flash0 check: EOF %08x, past EOF %08x.\n", (unsigned int)eof, (unsigned int)beyond);
    return 0;
}

static int confirm_flash_data_end(SceUID fd, const unsigned char *boot, const ProxFlashCapacity *capacity,
                                  uint32_t length, void *sector)
{
    /* FAT can leave fewer than one cluster of unused sectors at volume end.
     * A complete sector read at the end of the last allocatable cluster is
     * enough to check the full allocation area when only that padding fails. */
    uint32_t root_bytes = flash_le16(boot + 17) * 32;
    uint32_t root_sectors = (root_bytes + PROX_FLASH_SECTOR_BYTES - 1) / PROX_FLASH_SECTOR_BYTES;
    uint32_t data_offset = (flash_le16(boot + 14) + boot[16] * flash_le16(boot + 22) + root_sectors) *
                           PROX_FLASH_SECTOR_BYTES;
    uint32_t data_end = data_offset + capacity->total_bytes;
    if (data_end > length || length - data_end >= capacity->cluster_bytes)
        return 0;
    pspDebugScreenPrintf("flash0 check: FAT data end %08x.\n", (unsigned int)data_end);
    if (data_end == length || read_flash_sectors(&fd, data_end - PROX_FLASH_SECTOR_BYTES,
                                                sector, PROX_FLASH_SECTOR_BYTES) < 0)
        return 0;
    pspDebugScreenPrintf("flash0 check: verified FAT data extent.\n");
    return 1;
}

/* A return of 1 means the FAT is valid but still needs a partition extent. */
static int read_flash_tables(SceUID fd, unsigned char *boot, ProxFlashCapacity *capacity)
{
    unsigned char check[PROX_FLASH_SECTOR_BYTES] __attribute__((aligned(64)));
    int result = read_flash_sectors(&fd, 0, boot, sizeof(check));
    if (result < 0)
        return capacity_error("read flash0 boot sector", result);

    uint32_t sectors = boot_sector_count(boot);
    if (boot[510] != 0x55 || boot[511] != 0xaa || boot[11] != 0 || boot[12] != 2 ||
        boot[16] != 2 || !sectors || sectors > PROX_FLASH_MAX_VOLUME_BYTES / PROX_FLASH_SECTOR_BYTES)
        return capacity_error("validate flash0 boot sector", -1);

    uint32_t length = sectors * PROX_FLASH_SECTOR_BYTES;
    SceOff device_end = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (device_end > 0) {
        if (device_end < (SceOff)length || device_end > PROX_FLASH_MAX_VOLUME_BYTES ||
            device_end % PROX_FLASH_SECTOR_BYTES)
            return capacity_error("validate flash0 device extent", -1);
        pspDebugScreenPrintf("flash0 check: device extent %u bytes.\n", (unsigned int)device_end);
    } else {
        capacity_error("query flash0 device extent", device_end < 0 ? (int)device_end : -1);
    }

    result = prox_flash_capacity(read_flash_sectors, &fd, device_end > 0 ? (uint32_t)device_end : length, capacity);
    if (result)
        return capacity_error("validate flash0 allocation tables", result);

    int tail_error = device_end <= 0 ? read_flash_sectors(&fd, length - sizeof(check), check, sizeof(check)) : 0;
    int needs_table = tail_error < 0 &&
                      !confirm_flash_seek_boundary(fd, length, tail_error, check) &&
                      !confirm_flash_data_end(fd, boot, capacity, length, check);

    result = read_flash_sectors(&fd, 0, check, sizeof(check));
    if (result < 0 || memcmp(boot, check, sizeof(check)))
        return capacity_error("recheck flash0 boot sector", result < 0 ? result : -1);
    return needs_table;
}

/* Flash0 is the first FAT partition in the logical flash. Sony layouts can
 * place it in the first EBR. Seek only to the tables and the known boot sector. */
static int partition_entry(const unsigned char *entry, uint32_t parent_size, uint32_t *start, uint32_t *count)
{
    *start = flash_le32(entry + 8);
    *count = flash_le32(entry + 12);
    if ((entry[0] != 0 && entry[0] != 0x80) || !*start || *start >= parent_size ||
        !*count || *count > parent_size - *start)
        return -1;
    return entry[4];
}

static int first_partition(const char *name, const unsigned char *table, uint32_t parent_size,
                           uint32_t *start, uint32_t *count)
{
    if (table[510] != 0x55 || table[511] != 0xaa) {
        pspDebugScreenPrintf("flash0 check: %s signature %04x.\n", name, (unsigned int)flash_le16(table + 510));
        return -1;
    }
    const unsigned char *first = table + 446;
    int type = partition_entry(first, parent_size, start, count);
    pspDebugScreenPrintf("flash0 check: %s type %02x, start %u, size %u.\n", name, (unsigned int)first[4],
                         (unsigned int)*start, (unsigned int)*count);
    if (type <= 0)
        return -1;
    for (unsigned int i = 1; i < 4; ++i) {
        const unsigned char *entry = first + i * 16;
        uint32_t other_start, other_count;
        if (entry[4] && (partition_entry(entry, parent_size, &other_start, &other_count) < 0 ||
            (other_start < *start + *count && *start < other_start + other_count))) {
            pspDebugScreenPrintf("flash0 check: %s entry %u invalid.\n", name, i + 1);
            return -1;
        }
    }
    return type;
}

static int match_flash_partition(SceUID fd, const unsigned char *boot)
{
    unsigned char sector[PROX_FLASH_SECTOR_BYTES] __attribute__((aligned(64)));
    uint32_t start, count;
    int result = read_flash_sectors(&fd, 0, sector, sizeof(sector));
    if (result < 0)
        return result;
    if (!memcmp(boot, sector, sizeof(sector)))
        return capacity_error("lflash view starts at flash0 boot", -1);
    int type = first_partition("MBR", sector, PROX_FLASH_MAX_VOLUME_BYTES / PROX_FLASH_SECTOR_BYTES, &start, &count);
    if (type == 0x05 || type == 0x0f || type == 0x85) {
        uint32_t relative;
        result = read_flash_sectors(&fd, start * PROX_FLASH_SECTOR_BYTES, sector, sizeof(sector));
        if (result < 0)
            return result;
        type = first_partition("EBR", sector, count, &relative, &count);
        if (type < 0)
            return type;
        start += relative;
    }
    if (type != 0x01 && type != 0x04 && type != 0x06 && type != 0x0e)
        return capacity_error("find first FAT partition", -1);
    if (count < boot_sector_count(boot))
        return capacity_error("partition shorter than FAT volume", -1);

    result = read_flash_sectors(&fd, start * PROX_FLASH_SECTOR_BYTES, sector, sizeof(sector));
    if (result < 0)
        return result;
    return memcmp(boot, sector, sizeof(sector)) ? capacity_error("match partition boot sector", -1) : 0;
}

enum RawFlashResult { RAW_FLASH_MEASURED, RAW_FLASH_UNAVAILABLE, RAW_FLASH_CLOSE_FAILED };

static enum RawFlashResult measure_partition_extent(const unsigned char *boot)
{
    SceUID fd = sceIoOpen("lflash:", PSP_O_RDONLY, 0);
    if (fd < 0) {
        capacity_error("open flash partition table", fd);
        return RAW_FLASH_UNAVAILABLE;
    }
    int matched = match_flash_partition(fd, boot);
    int closed = sceIoClose(fd);
    if (closed < 0) {
        capacity_error("close flash partition table", closed);
        return RAW_FLASH_CLOSE_FAILED;
    }
    if (matched < 0) {
        capacity_error("validate flash0 partition extent", matched);
        return RAW_FLASH_UNAVAILABLE;
    }
    pspDebugScreenPrintf("flash0 check: verified partition table extent.\n");
    return RAW_FLASH_MEASURED;
}

static enum RawFlashResult measure_raw_flash(ProxFlashCapacity *capacity)
{
    SceUID fd = sceIoOpen("lflash0:0,0", PSP_O_RDONLY, 0);
    if (fd < 0) {
        capacity_error("open flash0 read-only", fd);
        return RAW_FLASH_UNAVAILABLE;
    }
    unsigned char boot[PROX_FLASH_SECTOR_BYTES] __attribute__((aligned(64)));
    int measured = read_flash_tables(fd, boot, capacity);
    int closed = sceIoClose(fd);
    if (closed < 0) {
        capacity_error("close read-only flash0", closed);
        return RAW_FLASH_CLOSE_FAILED;
    }
    if (measured < 0)
        return RAW_FLASH_UNAVAILABLE;
    return measured ? measure_partition_extent(boot) : RAW_FLASH_MEASURED;
}

static int start_flash_helper(int module)
{
    /* The tagged nonzero result tells Module Manager to unload the one-shot
     * module. A failed start may leave its loaded image to be unloaded here. */
    uint32_t request = PROX_CAPACITY_REQUEST_TAG;
    int status = -1;
    int started = sceKernelStartModule(module, sizeof(request), &request, &status, NULL);
    if (started == module) {
        int stopped = sceKernelStopModule(module, 0, NULL, NULL, NULL);
        if (stopped != 0)
            return capacity_error("stop helper", stopped);
        int unloaded = sceKernelUnloadModule(module);
        if (unloaded < 0)
            return capacity_error("unload helper", unloaded);
    } else if (started < 0) {
        sceKernelUnloadModule(module);
        return capacity_error("start staged helper", started);
    }
    if (((unsigned int)status & PROX_CAPACITY_RESULT_MASK) != PROX_CAPACITY_RESULT_TAG)
        return capacity_error("helper result", status);
    return status;
}

static int measure_flash_helper(const char *installer_path)
{
    char path[256], hex[65];
    unsigned char digest[32];
    const char *slash = installer_path ? strrchr(installer_path, '/') : NULL;
    if (!slash || strncmp(installer_path, "ms0:/", 5) || strcmp(slash, "/installer.prx"))
        return capacity_error("installer path", -1);
    size_t prefix = (size_t)(slash - installer_path) + 1;
    if (prefix + sizeof(PROX_CAPACITY_TEMP_FILE) > sizeof(path))
        return capacity_error("helper path length", -1);
    memcpy(path, installer_path, prefix);
    memcpy(path + prefix, PROX_CAPACITY_TEMP_FILE, sizeof(PROX_CAPACITY_TEMP_FILE));

    /* Keep the exclusively created and verified file through module start. */
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_EXCL, 0600);
    if (fd < 0)
        return capacity_error("create helper on Memory Stick", fd);
    int written = sceIoWrite(fd, flash_capacity_prx, size_flash_capacity_prx);
    int closed = sceIoClose(fd);
    int module = -1;
    if (written != (int)size_flash_capacity_prx) {
        module = capacity_error("write helper", written);
    } else if (closed < 0) {
        module = capacity_error("close helper", closed);
    } else {
        int verified = mbedtls_sha256(flash_capacity_prx, size_flash_capacity_prx, digest, 0);
        if (!verified) {
            for (unsigned int i = 0; i < sizeof(digest); ++i)
                snprintf(hex + 2 * i, 3, "%02x", digest[i]);
            verified = prox_file_check(path, size_flash_capacity_prx, hex, 0);
        }
        if (verified) {
            module = capacity_error("verify helper", verified);
        } else {
            module = kuKernelLoadModule(path, 0, NULL);
            if (module < 0)
                capacity_error("load helper", module);
        }
    }
    int result = module < 0 ? module : start_flash_helper(module);
    int removed = sceIoRemove(path);
    return removed < 0 ? capacity_error("remove helper", removed) : result;
}

static uint32_t allocated_bytes(uint32_t bytes, uint32_t cluster_bytes)
{
    return (bytes + cluster_bytes - 1) & ~(cluster_bytes - 1);
}

static int measure_flash(const char *installer_path, uint32_t *cluster_bytes, uint32_t *free_bytes)
{
    int result = measure_mounted_flash(cluster_bytes, free_bytes);
    if (!result) {
        pspDebugScreenPrintf("flash0 check: using mounted FAT capacity.\n");
        return 0;
    }
    capacity_error("query mounted FAT", result);

    ProxFlashCapacity capacity;
    enum RawFlashResult raw = measure_raw_flash(&capacity);
    if (raw == RAW_FLASH_MEASURED) {
        *cluster_bytes = capacity.cluster_bytes;
        *free_bytes = capacity.free_bytes;
        pspDebugScreenPrintf("flash0 check: using raw FAT capacity.\n");
        return 0;
    }
    if (raw == RAW_FLASH_CLOSE_FAILED)
        return -1;

    result = measure_flash_helper(installer_path);
    if (result < 0)
        return result;
    uint32_t cluster = PROX_FLASH_SECTOR_BYTES << ((result >> 16) & 7);
    uint64_t measured_free = (uint64_t)(result & 0xffff) * cluster;
    if (measured_free > PROX_FLASH_MAX_VOLUME_BYTES)
        return -1;
    *cluster_bytes = cluster;
    *free_bytes = (uint32_t)measured_free;
    return 0;
}

int prox_resident_capacity_ready(const char *installer_path, const uint32_t *module_sizes,
                                 unsigned int module_count)
{
    uint32_t cluster_bytes, free_bytes;
    int result = measure_flash(installer_path, &cluster_bytes, &free_bytes);
    if (result < 0) {
        pspDebugScreenPrintf("Could not measure flash0 free space (0x%08x).\n", (unsigned int)result);
        pspDebugScreenPrintf("No firmware write started.\n");
        return -1;
    }
    /* Budget full target copies without credit for existing files. Each of
     * our filenames fits in at most three 32-byte FAT directory entries.
     * Round the combined entries up and reserve another cluster because
     * the entries are split across kd and vsh/module. */
    uint32_t directory_bytes = allocated_bytes((module_count + 1) * 3 * 32, cluster_bytes) + cluster_bytes;
    uint64_t required = directory_bytes + allocated_bytes(PROX_RESIDENT_IMAGE_SIZE, cluster_bytes);
    for (unsigned int i = 0; i < module_count; ++i) {
        if (!module_sizes[i] || module_sizes[i] > PROX_FLASH_MAX_VOLUME_BYTES)
            return -1;
        required += allocated_bytes(module_sizes[i], cluster_bytes);
    }
    pspDebugScreenPrintf("flash0 free: %u bytes, required: %u (cluster: %u).\n",
                         (unsigned int)free_bytes, (unsigned int)required, (unsigned int)cluster_bytes);
    if (free_bytes < required) {
        pspDebugScreenPrintf("Not enough flash0 space. No firmware write started.\n");
        return -1;
    }
    return 0;
}
