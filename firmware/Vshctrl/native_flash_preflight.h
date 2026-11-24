#ifndef PROX_NATIVE_FLASH_PREFLIGHT_H
#define PROX_NATIVE_FLASH_PREFLIGHT_H
#include "prox_native_install.h"

int prox_native_flash_preflight(const ProxNativePart *parts);
int prox_native_flash_preflight_dedicated(const ProxNativePart *parts, const ProxNativePart *worker);
int prox_native_flash_capacity_preview(void);
#endif
