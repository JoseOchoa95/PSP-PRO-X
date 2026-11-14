/* PSP storage/power/launch adapters. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psploadexec_kernel.h>
#include "systemctrl.h"
#include "update.h"
#include "transport.h"
#include "policy.h"
#include <stdio.h>
#include <string.h>

int prox_new_stage(char root[192],const char *directory,const char *build_id)
{
    char candidate[192]; int n;
    if (strcmp(directory,"ms0:/PSP/GAME/PROXUPDATE") && strcmp(directory,"ef0:/PSP/GAME/PROXUPDATE")) return -1;
    n=snprintf(candidate,sizeof(candidate),"%s/.stage-%s-%08x",directory,build_id,sceKernelGetSystemTimeLow());
    if (n < 0 || n >= (int)sizeof(candidate) || sceIoMkdir(candidate,0700) < 0) return -1;
    strcpy(root,candidate); return 0;
}
int prox_launch_job(const ProxJob *job)
{
    struct SceKernelLoadExecVSHParam param;
    char executable[256],arguments[400]; size_t used; unsigned int source[3];
    if (!job->root[0] || !prox_hash_text(job->manifest_sha256) ||
        prox_version(job->source_version,source) ||
        snprintf(executable,sizeof(executable),"%s/PROUPDATE/EBOOT.PBP",job->root) >= (int)sizeof(executable)) return -1;
    used=strlen(executable)+1; memcpy(arguments,executable,used);
    memcpy(arguments+used,"--prox-ota-v1",14); used+=14;
    memcpy(arguments+used,job->manifest_sha256,65); used+=65;
    memcpy(arguments+used,job->source_version,strlen(job->source_version)+1); used+=strlen(job->source_version)+1;
    memset(&param,0,sizeof(param)); param.size=sizeof(param); param.args=used;
    param.argp=arguments; param.key="game";
    return !strncmp(executable,"ef0:/",5) ? sctrlKernelLoadExecVSHEf2(executable,&param) :
                                         sctrlKernelLoadExecVSHMs2(executable,&param);
}
