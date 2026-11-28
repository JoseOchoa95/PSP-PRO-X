/* A fixed request word is copied by Module Manager. Capacity is returned as
 * the module-start status, with no caller output buffer. */
#ifndef PROX_INSTALLER_FLASH_CAPACITY_RESULT_H
#define PROX_INSTALLER_FLASH_CAPACITY_RESULT_H

#define PROX_CAPACITY_REQUEST_TAG 0x50434150u
#define PROX_CAPACITY_RESULT_TAG 0x50400000u
#define PROX_CAPACITY_RESULT_MASK 0xfff80000u
#define PROX_CAPACITY_TEMP_FILE "prox-flash-capacity.tmp.prx"

#endif
