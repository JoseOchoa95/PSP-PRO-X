/* PRO-X, GPL-3.0-or-later. Internal only. */
#ifndef PROX_WPA2_H
#define PROX_WPA2_H
#include "wpa2_patch.h"
void prox_wpa2_init_config(void);
enum Wpa2Result prox_wpa2_module(SceModule2 *mod);
#endif
