#include "resident_completion.h"
#include "prox_completion_preview.h"
#include "stage.h"
#include <string.h>
#ifdef PROX_RESIDENT_COMPLETION_TEST
#include "resident_loader_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#endif

static const int file_missing = (int)0x80010002;

static int store_matches(void)
{
    return !prox_file_check(PROX_COMPLETION_STORE_PATH, PROX_COMPLETION_STORE_BYTES,
                            PROX_COMPLETION_STORE_SHA256, 0);
}

int prox_resident_completion_prepare(const char *installer_path, unsigned char store[PROX_COMPLETION_STORE_BYTES])
{
    SceIoStat stat;
    int existing = sceIoGetstat(PROX_COMPLETION_STORE_PATH, &stat);
    if (existing != file_missing)
        return existing >= 0 && FIO_S_ISREG(stat.st_mode) && store_matches() ? 0 : -1;

    char resource[256];
    const char *end = installer_path ? strrchr(installer_path, '/') : NULL;
    if (!end || (strncmp(installer_path, "ms0:/", 5) && strncmp(installer_path, "ef0:/", 5)) ||
        strcmp(end, "/installer.prx") || (size_t)(end - installer_path) + sizeof("/native-completion.rco") > sizeof(resource))
        return -1;
    snprintf(resource, sizeof(resource), "%.*s/native-completion.rco", (int)(end - installer_path), installer_path);
    int status = sceIoGetstat(resource, &stat);
    if (status == file_missing)
        return 0;
    if (status < 0 || !FIO_S_ISREG(stat.st_mode) ||
        prox_file_check(resource, PROX_COMPLETION_RCO_SIZE, PROX_COMPLETION_RCO_SHA256, 0))
        return -1;

    FILE *file = fopen(resource, "rb");
    if (!file)
        return -1;
    int result = prox_completion_pack(file, store);
    return fclose(file) || result ? -1 : 1;
}

int prox_resident_completion_install(const unsigned char store[PROX_COMPLETION_STORE_BYTES])
{
    if (!prox_completion_store_valid(store))
        return -1;
    int fd = sceIoOpen(PROX_COMPLETION_STORE_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_EXCL, 0600);
    if (fd < 0)
        return -1;
    int written = sceIoWrite(fd, store, PROX_COMPLETION_STORE_BYTES);
    if (sceIoClose(fd) < 0 || written != (int)PROX_COMPLETION_STORE_BYTES || !store_matches()) {
        sceIoRemove(PROX_COMPLETION_STORE_PATH);
        return -1;
    }
    return 0;
}
