#include "resident_loader_shim.h"
#include "prox_resident_image.h"
#include <assert.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

int prox_provision_resident_image(void);
int prox_native_resident_load(void);

static unsigned char disk[PROX_RESIDENT_IMAGE_SIZE + 1];
static unsigned int file_size, cursor, allocs, releases, loads, removes;
static unsigned char *allocation;
static int exists, not_regular, short_write, short_read, error_close, missing_export, bad_version;
static int allocated, opened, flushed, metadata_error, reported_extra;

unsigned int sceKernelDevkitVersion(void)
{
    return bad_version ? 0x06060010 : 0x06060110;
}

void sceKernelDcacheWritebackInvalidateAll(void)
{
    assert(allocated && !opened);
    flushed = 1;
}

int sceIoGetstat(const char *path, SceIoStat *stat)
{
    assert(!strcmp(path, PROX_RESIDENT_IMAGE_PATH));
    if (metadata_error) return -13;
    if (!exists) return (int)0x80010002;
    stat->st_size = reported_extra ? PROX_RESIDENT_IMAGE_SIZE : file_size;
    stat->st_mode = not_regular ? 2 : 1;
    return 0;
}

int sceIoOpen(const char *path, int flags, int mode)
{
    assert(!strcmp(path, PROX_RESIDENT_IMAGE_PATH) && !opened);
    cursor = 0;
    if (flags == PSP_O_RDONLY) {
        assert(exists && mode == 0);
        return opened = 1;
    }
    assert(mode == 0600);
    assert(flags == (PSP_O_WRONLY | PSP_O_CREAT | (exists ? PSP_O_TRUNC : PSP_O_EXCL)));
    exists = 1;
    file_size = 0;
    return opened = 2;
}

int sceIoRead(int fd, void *data, unsigned int size)
{
    assert(opened == 1 && fd == 1);
    if (size > file_size - cursor)
        size = file_size - cursor;
    if (short_read && size)
        size--;
    memcpy(data, disk + cursor, size);
    cursor += size;
    return size;
}

int sceIoWrite(int fd, const void *data, unsigned int size)
{
    assert(opened == 2 && fd == 2 && cursor + size <= PROX_RESIDENT_IMAGE_SIZE);
    if (short_write && size)
        size--;
    memcpy(disk + cursor, data, size);
    cursor += size;
    file_size = cursor;
    return size;
}

int sceIoClose(int fd)
{
    assert(opened && fd == opened);
    opened = 0;
    return error_close ? -1 : 0;
}

int sceIoRemove(const char *path)
{
    assert(!strcmp(path, PROX_RESIDENT_IMAGE_PATH) && !opened);
    exists = 0;
    removes++;
    return 0;
}

int prox_file_check(const char *path, unsigned int size, const char *expected, int pbp)
{
    unsigned char hash[32];
    char hex[65];

    assert(!strcmp(path, PROX_RESIDENT_IMAGE_PATH) && !pbp && !opened);
    assert(size == PROX_RESIDENT_IMAGE_SIZE);
    if (!exists || file_size != size || mbedtls_sha256(disk, size, hash, 0))
        return -1;
    for (unsigned int i = 0; i < sizeof(hash); ++i)
        snprintf(hex + 2*i, 3, "%02x", hash[i]);
    return strcmp(hex, expected);
}

static int allocate(int partition, const char *label, int type, unsigned int size, void *unused)
{
    assert(partition == 2 && strstr(label, "PRO-X") && type == PSP_SMEM_High);
    assert(size == PROX_RESIDENT_IMAGE_SIZE + 63 && !unused && !allocated);
    allocs++;
    allocated = 1;
    flushed = 0;
    return 17;
}

static void *head(int id)
{
    assert(id == 17 && allocated);
    return allocation + 1;
}

static int release(int id)
{
    assert(id == 17 && allocated && !opened);
    allocated = 0;
    releases++;
    return 0;
}

static int load(SceSize size, void *data, int flags, SceKernelLMOption *option)
{
    assert(allocated && flushed && !opened && !flags && data == allocation + 64);
    assert(size == PROX_RESIDENT_IMAGE_SIZE && !memcmp(data, disk, size));
    assert(option->size == sizeof(*option) && option->access == 1 && option->position == 1);
    loads++;
    return 23;
}

void *sctrlHENFindFunction(const char *name, const char *library, unsigned int nid)
{
    if (nid == 0x4e62c48a) {
        assert(!strcmp(name, "sceModuleManager") && !strcmp(library, "ModuleMgrForKernel"));
        return missing_export ? NULL : load;
    }
    assert(!strcmp(name, "sceSystemMemoryManager") && !strcmp(library, "SysMemForKernel"));
    switch (nid) {
        case 0x237dbd4f: return allocate;
        case 0x9d9a5ba1: return head;
        case 0xb6d61d02: return release;
        default: assert(0); return NULL;
    }
}

int main(void)
{
    allocation = mmap((void *)0x09000000, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(allocation == (void *)0x09000000);
    assert(prox_native_resident_load() < 0 && !allocs);
    metadata_error = 1;
    assert(prox_provision_resident_image() < 0 && !exists && !opened);
    metadata_error = 0;
    short_write = 1;
    assert(prox_provision_resident_image() < 0 && !exists && removes == 1);
    short_write = 0;
    assert(!prox_provision_resident_image() && exists && file_size == PROX_RESIDENT_IMAGE_SIZE);
    assert(prox_native_resident_load() == 23 && loads == 1 && allocs == releases);

    error_close = 1;
    assert(!prox_provision_resident_image() && removes == 1);
    assert(prox_native_resident_load() < 0 && loads == 1 && allocs == releases);
    error_close = 0;
    short_read = 1;
    assert(prox_native_resident_load() < 0 && loads == 1 && allocs == releases);
    short_read = 0;
    disk[9] ^= 1;
    assert(prox_native_resident_load() < 0 && loads == 1 && allocs == releases);
    assert(!prox_provision_resident_image() && prox_native_resident_load() == 23 && loads == 2);

    file_size++;
    assert(prox_native_resident_load() < 0 && loads == 2);
    reported_extra = 1;
    assert(prox_native_resident_load() < 0 && loads == 2 && allocs == releases);
    reported_extra = 0;
    file_size--;
    not_regular = 1;
    assert(prox_provision_resident_image() < 0 && prox_native_resident_load() < 0 && exists);
    not_regular = 0;
    bad_version = 1;
    assert(prox_native_resident_load() < 0 && loads == 2);
    bad_version = 0;
    missing_export = 1;
    assert(prox_native_resident_load() < 0 && loads == 2 && allocs == releases);
    munmap(allocation, 4096);
    puts("PASS: exact resident provisioning and same-buffer load; failure cleanup and retry; no other paths opened");
}
