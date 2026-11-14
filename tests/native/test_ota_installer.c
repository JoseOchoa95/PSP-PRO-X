/* Tests installer preflight/storage functions, with NO flash-install function
 * or PSP loader. The simulated commit is explicitly not a firmware test. */
#include "ota_installer_shim.h"
#include "ota_installer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
static const char *mode;
static int power_checks,renames;
int sceCtrlPeekBufferPositive(SceCtrlData *pad,int count)
{
    (void)count; pad->Buttons=!strcmp(mode,"cancel-preflight") && power_checks >= 2 ? PSP_CTRL_RTRIGGER : 0;
    if (!strcmp(mode,"cancel-after-prepare")) {
        FILE *f=fopen("ms0:/PSP/GAME/PROXUPDATE/installed.pending","rb");
        if (f) { fclose(f); pad->Buttons=PSP_CTRL_RTRIGGER; }
    }
    return 1;
}
int sceIoChdir(const char *path) { (void)path; return 0; }
int sceIoRemove(const char *path) { return remove(path); }
int scePowerLock(int i) { (void)i; return !strcmp(mode,"lock-failure") ? -1 : 0; }
int scePowerUnlock(int i) { (void)i; return 0; }
int pspDebugScreenPrintf(const char *fmt,...)
{ int n; va_list args; va_start(args,fmt); n=vprintf(fmt,args); va_end(args); return n; }
int prox_power_ready(void) { ++power_checks; return strcmp(mode,"power-lost") || power_checks < 2; }
int prox_storage_space(const char *p,unsigned long long n)
{ (void)p; (void)n; return !strcmp(mode,"space-short") ? -1 : 0; }
int prox_native_rename(const char *old,const char *new_name)
{
    ++renames;
    if (!strcmp(mode,"rename-failure") && renames == 4) { errno=EIO; return -1; }
    return rename(old,new_name);
}
int main(int argc,char **argv)
{
    char *args[4]; int rc;
    if (argc != 6) return 3;
    mode=argv[4]; args[0]=argv[1]; args[1]="--prox-ota-v1"; args[2]=argv[2]; args[3]=argv[3];
    puts("ISOLATED FIXTURE: NO FLASH INSTALLATION CAPABILITY");
    rc=prox_ota_begin(4,args,0x06060110,1);
    if (rc == 1 && strcmp(argv[5],"begin")) {
        rc=prox_ota_before_write();
        if (!rc && !strcmp(argv[5],"commit")) rc=prox_ota_complete();
    } else rc=rc == 1 ? 0 : -1;
    prox_ota_close(); printf("INSTALLER_FIXTURE rc=%d\n",rc); return rc ? 1 : 0;
}
