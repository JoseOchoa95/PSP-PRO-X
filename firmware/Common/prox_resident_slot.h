/* Derive a fixed flash0 slot from a validated PRO-X build ID. */
#ifndef PROX_RESIDENT_SLOT_H
#define PROX_RESIDENT_SLOT_H
#include <string.h>

#define PROX_RESIDENT_SLOT_PREFIX "flash0:/kd/_proxq-"
#define PROX_RESIDENT_SLOT_BYTES (sizeof(PROX_RESIDENT_SLOT_PREFIX) + 12u + 4u)

static inline int prox_resident_build_id(const char *build_id)
{
    if (strncmp(build_id, "pro-x-", 6))
        return -1;
    for (unsigned int i = 6; i < 18; ++i) {
        char digit = build_id[i];
        if (!digit || !strchr("0123456789abcdef", digit))
            return -1;
    }
    return build_id[18] ? -1 : 0;
}

static inline int prox_resident_slot(const char *build_id, char path[PROX_RESIDENT_SLOT_BYTES])
{
    if (prox_resident_build_id(build_id))
        return -1;
    memcpy(path, PROX_RESIDENT_SLOT_PREFIX, sizeof(PROX_RESIDENT_SLOT_PREFIX) - 1);
    memcpy(path + sizeof(PROX_RESIDENT_SLOT_PREFIX) - 1, build_id + 6, 12);
    memcpy(path + sizeof(PROX_RESIDENT_SLOT_PREFIX) - 1 + 12, ".prx", 5);
    return 0;
}
#endif
