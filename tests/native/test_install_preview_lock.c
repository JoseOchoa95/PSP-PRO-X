#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "prox_install_preview.h"
int main(void)
{
    unsigned int start=0;
    assert(prox_install_preview_path("ms0:/PSP/GAME/PROXUPDATE"));
    assert(prox_install_preview_path("ef0:/PSP/GAME/PROXUPDATE"));
    assert(!prox_install_preview_path("ms0:/PSP/GAME/PROXUPDATE/../UPDATE"));
    assert(!prox_install_preview_path(NULL));
    assert(!prox_install_preview_lock(100,&start,0) && !start);
    assert(prox_install_preview_lock(100,&start,1));
    assert(prox_install_preview_lock(PROX_INSTALL_PREVIEW_LOCK_US+99,&start,1));
    assert(!prox_install_preview_lock(PROX_INSTALL_PREVIEW_LOCK_US+100,&start,1));
    assert(!prox_install_preview_lock(PROX_INSTALL_PREVIEW_LOCK_US+10000,&start,1));
    assert(!prox_install_preview_lock(20000001,&start,0) && !start);
    assert(prox_install_preview_lock(0xfffffff0u,&start,1));
    assert(prox_install_preview_lock(0x00001000u,&start,1));
    assert(!prox_install_preview_lock(0xfffffff0u+PROX_INSTALL_PREVIEW_LOCK_US,&start,1));
    puts("PASS: only exact preview roots; failed preview control lock expires across clock wrap and resets after unload");
}
