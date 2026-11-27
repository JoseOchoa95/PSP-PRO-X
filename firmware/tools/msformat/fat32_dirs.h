/* GPL-3.0-or-later */
#ifndef MSFORMAT_FAT32_DIRS_H
#define MSFORMAT_FAT32_DIRS_H

#include "fat32_ram.h"

enum { PSP_INITIAL_DIRECTORY_COUNT = 17 };
const char *fat32_trial_directory(unsigned index);
int fat32_ram_add_indicators(struct Fat32Ram *volume);
int fat32_ram_name_partition(struct Fat32Ram *volume, unsigned number);
int fat32_ram_finish_directories(struct Fat32Ram *volume);

#endif
