/* Private, matched no-file-write access proof. GPL-3.0-or-later. */
#ifndef PROX_MOUNT_ACCESS_H
#define PROX_MOUNT_ACCESS_H
#include <stdint.h>
#define PROX_NI_ACCESS_PROBE 8
#define PROX_NI_ACCESS_CAPS 9
#define PROX_NI_ACCESS_REVISION 3
#define PROX_NI_ACCESS_BEGIN 10
#define PROX_NI_ACCESS_END 11
typedef struct {
    uint32_t base,table,mutex_address,flags,references;
    int mutex,fd;
} ProxMountView;
typedef struct {
    int result,stage,open_result,lock_result,close_result,unlock_result;
    int original_fd,owned_fd,retained_fd,restored;
    uint32_t flags_before,flags_during,flags_after,refs_before,refs_after;
    int64_t original_cursor,owned_cursor;
    int active,flush_result;
} ProxMountReport;
#endif
