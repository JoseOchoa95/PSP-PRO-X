#define _GNU_SOURCE
/* Isolated files. The exact original resource is an optional private input. */
#include "resident_loader_shim.h"
#include "resident_completion.h"
#include "native_completion_text.h"
#include "prox_completion_preview.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char installer[] = "ms0:/PSP/GAME/PROUPDATE/installer.prx";
static const char resource[] = "ms0:/PSP/GAME/PROUPDATE/native-completion.rco";
static int write_calls, partial_write;

int prox_stopping(void) { return 0; }
void prox_log(const char *text, ...) { (void)text; }
int sceIoGetstat(const char *path, SceIoStat *info)
{
    struct stat observed;
    assert(!strcmp(path, resource) || !strcmp(path, PROX_COMPLETION_STORE_PATH));
    if (stat(path, &observed))
        return errno == ENOENT ? (int)0x80010002 : -1;
    info->st_mode = S_ISREG(observed.st_mode) ? 1 : 0;
    info->st_size = observed.st_size;
    return 0;
}
int sceIoOpen(const char *path, int flags, int permissions)
{
    assert(!strcmp(path, PROX_COMPLETION_STORE_PATH));
    assert(flags == (PSP_O_WRONLY | PSP_O_CREAT | PSP_O_EXCL) && permissions == 0600);
    ++write_calls;
    return open(path, O_WRONLY | O_CREAT | O_EXCL, permissions);
}
int sceIoWrite(int fd, const void *data, unsigned int count)
{
    return (int)write(fd, data, count - partial_write);
}
int sceIoClose(int fd) { return close(fd); }
int sceIoRemove(const char *path) { assert(!strcmp(path, PROX_COMPLETION_STORE_PATH)); return unlink(path); }

static void roundtrip(const char *original, unsigned char store[PROX_COMPLETION_STORE_BYTES])
{
    FILE *from = fopen(original, "rb"), *to = fopen(resource, "wb");
    unsigned char buffer[4096];
    size_t count;
    assert(from && to);
    while ((count = fread(buffer, 1, sizeof(buffer), from)) != 0)
        assert(fwrite(buffer, 1, count, to) == count);
    assert(!ferror(from) && !fclose(to));
    assert(prox_resident_completion_prepare(installer, store) == 1);
    assert(prox_completion_store_valid(store) && !write_calls);

    partial_write = 1;
    assert(prox_resident_completion_install(store) < 0);
    assert(write_calls == 1 && access(PROX_COMPLETION_STORE_PATH, F_OK));
    partial_write = 0;
    assert(!prox_resident_completion_install(store) && write_calls == 2);
    assert(!unlink(resource));
    assert(!prox_resident_completion_prepare(installer, store) && write_calls == 2);
    assert(prox_resident_completion_install(store) < 0 && write_calls == 3);

    for (unsigned int language = 0; language < PROX_COMPLETION_LANGUAGES; ++language) {
        unsigned short original_text[PROX_COMPLETION_UNITS], installed_text[PROX_COMPLETION_UNITS];
        int expected = prox_completion_read_text(from, language, original_text, PROX_COMPLETION_UNITS);
        FILE *packed = fopen(PROX_COMPLETION_STORE_PATH, "rb");
        assert(packed && expected > 1);
        assert(prox_completion_read_store(packed, language, installed_text, PROX_COMPLETION_UNITS) == expected);
        assert(!memcmp(original_text, installed_text, (size_t)expected * sizeof(*original_text)));
        rewind(packed);
        assert(prox_completion_read_store(packed, language, installed_text, (unsigned int)expected - 1) < 0);
        assert(!fclose(packed));
    }
    assert(!fclose(from));

    FILE *packed = fopen(PROX_COMPLETION_STORE_PATH, "wb");
    assert(packed);
    store[PROX_COMPLETION_STORE_BYTES - 2] ^= 1;
    assert(fwrite(store, 1, PROX_COMPLETION_STORE_BYTES, packed) == PROX_COMPLETION_STORE_BYTES && !fclose(packed));
    assert(prox_resident_completion_prepare(installer, store) < 0 && write_calls == 3);
    puts("PASS: one-time exact 6.61 import, short-write cleanup, retry, all twelve languages after removal of private input, preserved conflicting store");
}

int main(int argc, char **argv)
{
    unsigned char store[PROX_COMPLETION_STORE_BYTES] = {0};
    char temp[] = "/tmp/prox-resident-completion-XXXXXX";
    assert(mkdtemp(temp) && !chdir(temp));
    assert(!mkdir("flash0:", 0700) && !mkdir("flash0:/kd", 0700));
    assert(!mkdir("ms0:", 0700) && !mkdir("ms0:/PSP", 0700) && !mkdir("ms0:/PSP/GAME", 0700));
    assert(!mkdir("ms0:/PSP/GAME/PROUPDATE", 0700));
    assert(!prox_resident_completion_prepare(installer, store));
    assert(prox_resident_completion_install(store) < 0 && !write_calls);
    FILE *invalid = fopen(resource, "wb");
    assert(invalid && !fclose(invalid));
    assert(prox_resident_completion_prepare(installer, store) < 0 && !write_calls);
    assert(!unlink(resource));
    puts("PASS: missing optional input allowed, invalid original/store rejected before any write");
    if (argc == 2)
        roundtrip(argv[1], store);
    return 0;
}
