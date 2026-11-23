/* Read-only native lifecycle diagnostic; never authorizes firmware writes. */
#ifndef PROX_INSTALL_PREVIEW_H
#define PROX_INSTALL_PREVIEW_H
#include <string.h>
#define PROX_INSTALL_PREVIEW_MODULE "ProxNativeInstallPreview"
#define PROX_INSTALL_PREVIEW_ARGUMENT "--prox-install-preview-v1"
#define PROX_INSTALL_PREVIEW_BYTES (1024u*1024u)
#define PROX_INSTALL_PREVIEW_LOCK_US 25000000u
static inline int prox_install_preview_path(const char *p)
{
    return p && (!strcmp(p,"ms0:/PSP/GAME/PROXUPDATE") ||
                 !strcmp(p,"ef0:/PSP/GAME/PROXUPDATE"));
}
/* A failed diagnostic must restore controls, even if its module remains loaded. */
static inline int prox_install_preview_lock(unsigned int now,unsigned int *start,int loaded)
{
    if(!loaded) { *start=0; return 0; }
    now|=1u;
    if(!*start) *start=now;
    return (unsigned int)(now-*start)<PROX_INSTALL_PREVIEW_LOCK_US;
}
#endif
