#include "resident_loader_shim.h"
#include "flash_capacity_result.h"
#include "flash_capacity_prx.h"
#include "prox_resident_image.h"
#include <assert.h>
#include <mbedtls/sha256.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int prox_resident_capacity_ready(const char *, const uint32_t *, unsigned int);

static const char installer[] = "ms0:/PSP/GAME/PROUPDATE/installer.prx";
static const char helper[] = "ms0:/PSP/GAME/PROUPDATE/" PROX_CAPACITY_TEMP_FILE;
static int exists, opened, fail_close, fail_validate, fail_load, fail_remove, fail_start;
static int starts, unloads, stops, writes, reported;
static int create_error, start_error, return_start_status;
static int mounted_result = -1;
static uint32_t mounted_volume[5];
static char failure[96];
static unsigned int cluster_shift = 5;
static unsigned int free_clusters = 7;
enum { RAW_SECTOR_BYTES = 512, RAW_VOLUME_SECTORS = 259, EBR_SECTOR = 32, PARTITION_SECTOR = 64 };
static unsigned char raw_metadata[3 * RAW_SECTOR_BYTES];
static SceOff raw_position, raw_length = RAW_VOLUME_SECTORS * RAW_SECTOR_BYTES;
static int raw_open_error = -1, raw_close_error, raw_open, raw_last_reads;
static int raw_end_unsupported, raw_last_error;
static int raw_seek_limit, seek_verified, data_verified;
static int raw_sony_boundary, sony_verified, raw_previous_error, raw_cur_error;
static unsigned int test_firmware = 0x06060110u;
static unsigned char disk_mbr[RAW_SECTOR_BYTES], disk_ebr[RAW_SECTOR_BYTES];
static SceOff disk_position;
static int disk_open_error = -1, disk_close_error, disk_open, disk_reads, disk_boot_mismatch;

unsigned int sceKernelDevkitVersion(void)
{
    return test_firmware;
}

int sceIoDevctl(const char *device, unsigned int command, void *input, int input_size,
                void *output, int output_size)
{
    assert(!strcmp(device, "flash0:") && command == 0x0003d814);
    assert(!input && !input_size && output && output_size == sizeof(mounted_volume));
    if (!mounted_result)
        memcpy(output, mounted_volume, sizeof(mounted_volume));
    return mounted_result;
}

int sceIoOpen(const char *path, int flags, int mode)
{
    if (!strcmp(path, "lflash:")) {
        assert(flags == PSP_O_RDONLY && !mode && !disk_open && !raw_open);
        if (disk_open_error)
            return disk_open_error;
        disk_open = 1;
        return 33;
    }
    if (!strcmp(path, "lflash0:0,0")) {
        assert(flags == PSP_O_RDONLY && !mode && !raw_open);
        if (raw_open_error)
            return raw_open_error;
        raw_open = 1;
        return 32;
    }
    assert(!strcmp(path, helper) && flags == (PSP_O_WRONLY | PSP_O_CREAT | PSP_O_EXCL) && mode == 0600);
    if (create_error)
        return create_error;
    if (exists)
        return -17;
    exists = opened = 1;
    return 31;
}

SceOff sceIoLseek(int fd, SceOff offset, int whence)
{
    if (fd == 33) {
        assert(disk_open && whence == PSP_SEEK_SET && offset >= 0);
        disk_position = offset;
        return offset;
    }
    assert(fd == 32 && raw_open);
    if (whence == PSP_SEEK_END) {
        assert(!offset);
        if (raw_end_unsupported)
            return -1;
        raw_position = raw_length;
        return raw_position;
    }
    if (whence == PSP_SEEK_CUR) {
        assert(raw_sony_boundary && !offset);
        return raw_cur_error ? raw_cur_error : raw_position;
    }
    assert(whence == PSP_SEEK_SET && offset >= 0);
    if (raw_sony_boundary && offset > raw_length - RAW_SECTOR_BYTES)
        return (int)0x80010016u;
    if (raw_seek_limit && offset > raw_length) {
        if (raw_seek_limit == 1)
            return (int)0x80010016u;
        offset = raw_length;
    }
    raw_position = offset;
    return offset;
}

int sceIoRead(int fd, void *bytes, unsigned int size)
{
    if (fd == 33) {
        assert(disk_open && size == RAW_SECTOR_BYTES);
        ++disk_reads;
        if (!disk_position)
            memcpy(bytes, disk_mbr, size);
        else if (disk_position == EBR_SECTOR * RAW_SECTOR_BYTES)
            memcpy(bytes, disk_ebr, size);
        else {
            assert(disk_position == PARTITION_SECTOR * RAW_SECTOR_BYTES);
            memcpy(bytes, raw_metadata, size);
            ((unsigned char *)bytes)[40] = disk_boot_mismatch;
        }
        return (int)size;
    }
    assert(fd == 32 && raw_open && size == RAW_SECTOR_BYTES);
    if (raw_previous_error && raw_position == raw_length - 2 * RAW_SECTOR_BYTES)
        return raw_previous_error;
    if (raw_position + size == raw_length) {
        ++raw_last_reads;
        if (raw_last_error)
            return raw_last_error;
    }
    if (raw_position + size > raw_length)
        return 0;
    if (raw_position < (SceOff)sizeof(raw_metadata))
        memcpy(bytes, raw_metadata + raw_position, size);
    else
        memset(bytes, 0, size);
    raw_position += size;
    return (int)size;
}

int sceIoWrite(int fd, const void *bytes, unsigned int size)
{
    assert(fd == 31 && opened && size == size_flash_capacity_prx);
    assert(!memcmp(bytes, flash_capacity_prx, size));
    ++writes;
    return size;
}

int sceIoClose(int fd)
{
    if (fd == 33) {
        assert(disk_open);
        disk_open = 0;
        return disk_close_error;
    }
    if (fd == 32) {
        assert(raw_open);
        raw_open = 0;
        return raw_close_error;
    }
    assert(fd == 31 && opened);
    opened = 0;
    return fail_close ? -1 : 0;
}

int sceIoRemove(const char *path)
{
    assert(!strcmp(path, helper) && exists && !opened);
    if (fail_remove)
        return -1;
    exists = 0;
    return 0;
}

int prox_file_check(const char *path, unsigned int size, const char *hex, int pbp)
{
    unsigned char digest[32];
    char expected[65];
    assert(!strcmp(path, helper) && exists && !opened && !pbp && size == size_flash_capacity_prx);
    assert(!mbedtls_sha256(flash_capacity_prx, size, digest, 0));
    for (unsigned int i = 0; i < sizeof(digest); ++i)
        snprintf(expected + 2 * i, 3, "%02x", digest[i]);
    assert(!strcmp(hex, expected));
    return fail_validate ? -1 : 0;
}

int kuKernelLoadModule(const char *path, int flags, SceKernelLMOption *option)
{
    assert(!strcmp(path, helper) && exists && !opened && !flags && !option);
    return fail_load ? fail_load : 42;
}

int sceKernelStartModule(int module, SceSize args, void *argp, int *status, SceKernelSMOption *option)
{
    assert(module == 42 && exists && !opened && args == sizeof(uint32_t) && argp && status && !option);
    uint32_t request;
    memcpy(&request, argp, sizeof(request));
    assert(request == PROX_CAPACITY_REQUEST_TAG);
    ++starts;
    if (start_error)
        return start_error;
    if (fail_start) {
        *status = 0;
        return module;
    }
    *status = PROX_CAPACITY_RESULT_TAG | (cluster_shift << 16) | free_clusters;
    return return_start_status ? *status : 0;
}

int sceKernelStopModule(int module, SceSize args, void *argp, int *status, SceKernelSMOption *option)
{
    assert(module == 42 && exists && !args && !argp && !status && !option);
    ++stops;
    return 0;
}

int sceKernelUnloadModule(int module)
{
    assert(module == 42 && exists);
    ++unloads;
    return 0;
}

void pspDebugScreenPrintf(const char *format, ...)
{
    seek_verified += strstr(format, "verified seek boundary") != NULL;
    data_verified += strstr(format, "verified FAT data extent") != NULL;
    sony_verified += strstr(format, "verified Sony sector boundary") != NULL;
    if (strstr(format, "flash0 free:"))
        ++reported;
    if (strstr(format, "flash0 check:")) {
        va_list args;
        va_start(args, format);
        vsnprintf(failure, sizeof(failure), format, args);
        va_end(args);
    }
}

static void partition(unsigned char *table, unsigned int index, unsigned char type, uint32_t start, uint32_t size)
{
    unsigned char *entry = table + 446 + index * 16;
    entry[4] = type;
    for (unsigned int i = 0; i < 4; ++i) {
        entry[8 + i] = start >> (i * 8);
        entry[12 + i] = size >> (i * 8);
    }
    table[510] = 0x55;
    table[511] = 0xaa;
}

int main(void)
{
    const uint32_t sizes[] = {1, 16385};
    assert(!prox_resident_capacity_ready(installer, sizes, 2) && starts == 1 && reported == 1);
    /* Worker (1 cluster), both modules (1 and 2), and 2 directory clusters. */
    free_clusters = 6;
    return_start_status = 1;
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    return_start_status = 0;
    free_clusters = 5;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);

    int before = starts;
    assert(prox_resident_capacity_ready("flash0:/installer.prx", sizes, 2) < 0);
    exists = 1;
    int previous_writes = writes;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && exists && writes == previous_writes);
    exists = 0;
    fail_close = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    fail_close = 0;
    fail_validate = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    fail_validate = 0;
    fail_load = -1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    fail_load = 0;
    assert(starts == before);
    fail_remove = 1;
    free_clusters = 7;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && exists && starts == before + 1);
    assert(unloads == 0 && strstr(failure, "remove helper failed"));
    fail_remove = exists = 0;
    fail_start = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && stops == 1 && unloads == 1 && !exists);
    fail_start = 0;
    cluster_shift = 7;
    free_clusters = 65535;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    const uint32_t small_files[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    cluster_shift = 0;
    free_clusters = 15; /* Eleven file clusters and four directory clusters. */
    assert(!prox_resident_capacity_ready(installer, small_files, 10));
    free_clusters = 14;
    assert(prox_resident_capacity_ready(installer, small_files, 10) < 0 && !exists);
    const int illegal_argument = (int)0x800200d2u;
    create_error = illegal_argument;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    assert(strstr(failure, "create helper on Memory Stick failed (0x800200d2)"));
    create_error = 0;
    fail_load = illegal_argument;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists);
    assert(strstr(failure, "load helper failed (0x800200d2)"));
    fail_load = 0;
    start_error = illegal_argument;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && !exists && unloads == 2);
    assert(strstr(failure, "start staged helper failed (0x800200d2)"));

    before = starts;
    previous_writes = writes;
    mounted_result = 0;
    mounted_volume[0] = 16384;
    mounted_volume[1] = 2560;
    mounted_volume[2] = 6;
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    assert(strstr(failure, "using mounted FAT capacity"));
    mounted_volume[2] = 5;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0);
    assert(starts == before && writes == previous_writes);

    mounted_volume[0] = 16383;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 1);
    mounted_volume[0] = 16384;
    mounted_volume[1] = UINT32_MAX;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 2);

    mounted_result = -1;
    raw_open_error = 0;
    raw_metadata[12] = 2;
    raw_metadata[13] = 32;
    raw_metadata[14] = 1;
    raw_metadata[16] = 2;
    raw_metadata[18] = 2;
    raw_metadata[19] = RAW_VOLUME_SECTORS & 0xff;
    raw_metadata[20] = RAW_VOLUME_SECTORS >> 8;
    raw_metadata[21] = 0xf8;
    raw_metadata[22] = 1;
    raw_metadata[510] = 0x55;
    raw_metadata[511] = 0xaa;
    for (unsigned int offset = RAW_SECTOR_BYTES; offset < sizeof(raw_metadata); offset += RAW_SECTOR_BYTES) {
        raw_metadata[offset] = 0xf8;
        raw_metadata[offset + 1] = 0xff;
        raw_metadata[offset + 2] = 0xff;
    }
    before = starts;
    previous_writes = writes;
    raw_last_error = (int)0x8001000du;
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    assert(strstr(failure, "using raw FAT capacity"));
    assert(starts == before && writes == previous_writes && !raw_open && raw_last_reads == 0);

    raw_close_error = -1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0);
    assert(strstr(failure, "close read-only flash0 failed"));
    assert(starts == before && !raw_open);
    raw_close_error = 0;

    raw_metadata[16] = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 1);
    assert(raw_last_reads == 0 && !raw_open);
    raw_metadata[16] = 2;
    raw_metadata[2 * RAW_SECTOR_BYTES + 3] = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 2 && !raw_open);
    raw_metadata[2 * RAW_SECTOR_BYTES + 3] = 0;
    raw_length -= RAW_SECTOR_BYTES;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 3 && !raw_open);
    assert(raw_last_reads == 0);
    raw_length += RAW_SECTOR_BYTES;
    raw_end_unsupported = 1;
    raw_last_error = 0;
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    assert(strstr(failure, "using raw FAT capacity") && raw_last_reads == 1);
    raw_last_error = (int)0x8001000du;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 4 && !raw_open);
    assert(raw_last_reads == 2 && !disk_open && disk_reads == 0);

    disk_open_error = 0;
    partition(disk_mbr, 0, 0x01, PARTITION_SECTOR, RAW_VOLUME_SECTORS);
    before = starts;
    previous_writes = writes;
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    assert(strstr(failure, "using raw FAT capacity") && disk_reads == 2 && !disk_open);
    partition(disk_mbr, 0, 0x05, EBR_SECTOR, 1024);
    partition(disk_ebr, 0, 0x01, PARTITION_SECTOR - EBR_SECTOR, RAW_VOLUME_SECTORS);
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    assert(disk_reads == 5 && !disk_open && starts == before && writes == previous_writes);

    disk_close_error = -1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0);
    assert(strstr(failure, "close flash partition table failed") && starts == before && !disk_open);
    disk_close_error = 0;
    disk_boot_mismatch = 1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 1 && !disk_open);
    disk_boot_mismatch = 0;
    partition(disk_ebr, 0, 0x01, PARTITION_SECTOR - EBR_SECTOR, RAW_VOLUME_SECTORS - 1);
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 2 && !disk_open);
    partition(disk_ebr, 0, 0x01, PARTITION_SECTOR - EBR_SECTOR, RAW_VOLUME_SECTORS);
    partition(disk_ebr, 1, 0x05, RAW_VOLUME_SECTORS, 1);
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 3 && !disk_open);

    disk_open_error = -1;
    before = starts;
    raw_seek_limit = 1;
    assert(!prox_resident_capacity_ready(installer, sizes, 2) && starts == before && seek_verified == 1);
    raw_seek_limit = 2;
    assert(!prox_resident_capacity_ready(installer, sizes, 2) && starts == before && seek_verified == 2);
    raw_length -= RAW_SECTOR_BYTES;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && starts == before + 1);
    assert(seek_verified == 2 && !data_verified);
    raw_length += 3 * RAW_SECTOR_BYTES;
    raw_metadata[19] = (RAW_VOLUME_SECTORS + 2) & 0xff;
    raw_seek_limit = 0;
    assert(!prox_resident_capacity_ready(installer, sizes, 2) && starts == before + 1);
    assert(seek_verified == 2 && data_verified == 1 && !disk_open && !raw_open);

    raw_length -= 2 * RAW_SECTOR_BYTES;
    raw_metadata[19] = RAW_VOLUME_SECTORS & 0xff;
    raw_sony_boundary = 1;
    before = starts;
    previous_writes = writes;
    assert(!prox_resident_capacity_ready(installer, sizes, 2));
    assert(sony_verified == 1 && starts == before && writes == previous_writes && !raw_open);
    test_firmware = 0x06060010u;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && sony_verified == 1);
    test_firmware = 0x06060110u;
    raw_previous_error = (int)0x8001000du;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && sony_verified == 1);
    raw_previous_error = 0;
    raw_cur_error = -1;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && sony_verified == 1);
    raw_cur_error = 0;
    raw_length -= RAW_SECTOR_BYTES;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0);
    raw_length += RAW_SECTOR_BYTES;
    raw_last_error = (int)0x8001001eu;
    assert(prox_resident_capacity_ready(installer, sizes, 2) < 0 && sony_verified == 1);
    assert(starts == before + 5 && !disk_open && !raw_open);
    puts("PASS: exact Sony 6.61 sector boundary, checked read cursor and unchanged space budget");
}
