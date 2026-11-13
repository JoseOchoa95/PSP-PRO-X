/* PRO-X, GPL-3.0-or-later. Shared by SystemControl and Recovery; no ABI export. */
#include "prox_network.h"
#include <string.h>
#include <pspiofilemgr.h>

#define TEMP PROX_NETWORK_PATH ".tmp"
#define BACKUP PROX_NETWORK_PATH ".bak"
#define NOT_FOUND ((int)0x80010002)

int prox_network_decode(const unsigned char *p, size_t size, int *enabled)
{
    *enabled = 1;
    if (size != PROX_NETWORK_SIZE || memcmp(p,"PXNW",4) ||
        p[4] != 1 || p[5] || p[6] || p[7] || p[8] > 1 ||
        p[9] || p[10] || p[11])
        return -1;
    *enabled = p[8];
    return 0;
}

void prox_network_encode(unsigned char p[PROX_NETWORK_SIZE], int enabled)
{
    memset(p,0,PROX_NETWORK_SIZE);
    memcpy(p,"PXNW",4);
    p[4] = 1;
    p[8] = !!enabled;
}

enum ProxNetworkStatus prox_network_read(int *enabled)
{
    unsigned char bytes[PROX_NETWORK_SIZE + 1];
    int fd, count, closed, recovered = 0;
    *enabled = 1;
    fd = sceIoOpen(PROX_NETWORK_PATH,PSP_O_RDONLY,0);
    if (fd == NOT_FOUND) {
        fd = sceIoOpen(BACKUP,PSP_O_RDONLY,0);
        recovered = 1;
    }
    if (fd < 0)
        return fd == NOT_FOUND ? PROX_NETWORK_MISSING : PROX_NETWORK_IO_ERROR;
    count = sceIoRead(fd,bytes,sizeof(bytes));
    closed = sceIoClose(fd);
    if (count < 0 || closed < 0) return PROX_NETWORK_IO_ERROR;
    if (prox_network_decode(bytes,(size_t)count,enabled)) return PROX_NETWORK_INVALID;
    return recovered ? PROX_NETWORK_RECOVERED : PROX_NETWORK_VALID;
}

int prox_network_save(int enabled)
{
    unsigned char bytes[PROX_NETWORK_SIZE];
    int fd, written, closed, ret, moved = 0;
    SceIoStat stat;
    prox_network_encode(bytes,enabled);
    fd = sceIoOpen(TEMP,PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC,0600);
    if (fd < 0) return fd;
    written = sceIoWrite(fd,bytes,sizeof(bytes));
    closed = sceIoClose(fd);
    if (written != sizeof(bytes) || closed < 0) {
        sceIoRemove(TEMP);
        return -1;
    }
    ret = sceIoSync("flash1:",0);
    if (ret < 0) { sceIoRemove(TEMP); return ret; }
    ret = sceIoGetstat(PROX_NETWORK_PATH,&stat);
    if (ret >= 0) {
        /* Remove only a stale backup while the current preference is intact. */
        ret = sceIoRemove(BACKUP);
        if (ret < 0 && ret != NOT_FOUND) { sceIoRemove(TEMP); return ret; }
        ret = sceIoRename(PROX_NETWORK_PATH,BACKUP);
        if (ret < 0) { sceIoRemove(TEMP); return ret; }
        moved = 1;
    } else if (ret != NOT_FOUND) {
        sceIoRemove(TEMP);
        return ret;
    }
    ret = sceIoRename(TEMP,PROX_NETWORK_PATH);
    if (ret < 0) {
        if (moved) sceIoRename(BACKUP,PROX_NETWORK_PATH);
        /* If restoration fails, read() still recovers the previous backup. */
        sceIoRemove(TEMP);
        return ret;
    }
    /* Keep the previous preference in .bak for interrupted rename recovery.
     * Only a successful rename changes the preference shown by Recovery. */
    return 0;
}
