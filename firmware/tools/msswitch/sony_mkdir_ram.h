/* GPL-3.0-or-later */
#ifndef MSSWITCH_SONY_MKDIR_RAM_H
#define MSSWITCH_SONY_MKDIR_RAM_H

struct SonyMkdirResult {
    int result, unmount, mutex_close;
    unsigned created;
    int boundary_rejected;
};

int sony_mkdir_ram_check(void);
struct SonyMkdirResult sony_mkdir_ram_run(const char *device, int fd, unsigned sectors, int one_directory);

#endif
