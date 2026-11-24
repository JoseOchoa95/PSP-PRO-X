/* Fixed flash0 inventory shared by the writer and its capacity check. */
#ifndef PROX_NATIVE_FLASH_PATHS_H
#define PROX_NATIVE_FLASH_PATHS_H
#include "config.h"

static const char *const prox_native_flash_paths[] = {
    PATH_SYSTEMCTRL, PATH_VSHCTRL, PATH_GALAXY, PATH_STARGATE, PATH_MARCH33,
    PATH_INFERNO, PATH_USBDEVICE, PATH_POPCORN, PATH_SATELITE, PATH_RECOVERY
};
#define PROX_NATIVE_FLASH_FILES (sizeof(prox_native_flash_paths) / sizeof(prox_native_flash_paths[0]))
#endif
