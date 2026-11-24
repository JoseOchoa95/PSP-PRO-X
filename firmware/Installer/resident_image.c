/* Provision only this diagnostic build's unused flash0 slot. GPL-3.0-or-later. */
#ifdef PROX_RESIDENT_LOADER_TEST
#include "resident_loader_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#endif
#include <stdio.h>
#include <zlib.h>
#include "stage.h"
#include "prox_resident_image.h"
#include "resident_archive.h"

static int installed_image_matches(void)
{
    return prox_file_check(PROX_RESIDENT_IMAGE_PATH, PROX_RESIDENT_IMAGE_SIZE,
                           PROX_RESIDENT_IMAGE_DIGEST, 0) == 0;
}

int prox_provision_resident_image(void)
{
    static unsigned char chunk[8192];
    z_stream stream = {0};
    SceIoStat stat;
    int fd = -1, rc = -1, result, status;

    status = sceIoGetstat(PROX_RESIDENT_IMAGE_PATH, &stat);
    if (status >= 0) {
        if (!FIO_S_ISREG(stat.st_mode))
            return -1;
        if (installed_image_matches())
            return 0;
    } else if (status != (int)0x80010002) {
        return -1;
    }

    if (inflateInit(&stream) != Z_OK)
        return -1;
    stream.next_in = resident_archive;
    stream.avail_in = size_resident_archive;
    /* Only an invalid file at this build's path is repairable. Older builds
     * have distinct slots. The VSH fails closed on an incomplete write. */
    fd = sceIoOpen(PROX_RESIDENT_IMAGE_PATH, PSP_O_WRONLY | PSP_O_CREAT |
                   (status < 0 ? PSP_O_EXCL : PSP_O_TRUNC), 0600);
    if (fd < 0)
        goto done;
    do {
        unsigned int before = stream.total_out;
        stream.next_out = chunk;
        stream.avail_out = sizeof(chunk);
        result = inflate(&stream, Z_NO_FLUSH);
        if ((result != Z_OK && result != Z_STREAM_END) ||
            stream.total_out > PROX_RESIDENT_IMAGE_SIZE)
            goto done;
        unsigned int amount = stream.total_out - before;
        if (!amount && result != Z_STREAM_END)
            goto done;
        if (amount && sceIoWrite(fd, chunk, amount) != (int)amount)
            goto done;
    } while (result != Z_STREAM_END);
    if (stream.total_out == PROX_RESIDENT_IMAGE_SIZE && !stream.avail_in)
        rc = 0;
done:
    inflateEnd(&stream);
    if (fd >= 0) {
        if (sceIoClose(fd) < 0)
            rc = -1;
        if (rc || !installed_image_matches()) {
            /* This file was opened for this build. Never touch prior slots. */
            sceIoRemove(PROX_RESIDENT_IMAGE_PATH);
            return -1;
        }
        printf("Resident query worker verified.\n");
    }
    return rc;
}
