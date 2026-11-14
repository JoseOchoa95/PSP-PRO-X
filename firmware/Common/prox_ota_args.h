/* Private OTA argument contract shared by launcher/installer. GPL-3.0-or-later. */
#ifndef PROX_OTA_ARGS_H
#define PROX_OTA_ARGS_H
#include <string.h>
static int prox_ota_hex(const char *s,size_t n)
{
    size_t i;
    for(i=0;i<n;++i) if (!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return 0;
    return 1;
}
static int prox_ota_args(int argc,char **argv,const char *suffix,char root[192])
{
    const size_t base=sizeof("ms0:/PSP/GAME/PROXUPDATE/")-1;
    const char *component; size_t n,length,i;
    if (argc <= 1) {
        /* Losing OTA arguments must never enable manual uninstall/forced reset
         * on a staged launch. Normal Games/PROUPDATE bootstrap is unaffected. */
        if (argc == 1 && argv && argv[0] &&
            (!strncmp(argv[0],"ms0:/PSP/GAME/PROXUPDATE/.stage-",sizeof("ms0:/PSP/GAME/PROXUPDATE/.stage-")-1) ||
             !strncmp(argv[0],"ef0:/PSP/GAME/PROXUPDATE/.stage-",sizeof("ef0:/PSP/GAME/PROXUPDATE/.stage-")-1))) return -1;
        return 0;
    }
    if (argc != 4 || !argv || !argv[0] || !argv[1] || !argv[2] || !argv[3] ||
        strcmp(argv[1],"--prox-ota-v1") || strlen(argv[2]) != 64 || !prox_ota_hex(argv[2],64)) return -1;
    if (!argv[3][0] || strlen(argv[3]) >= 40) return -1;
    for (i=0;argv[3][i];++i) if ((argv[3][i] < '0' || argv[3][i] > '9') && argv[3][i] != '.') return -1;
    length=strlen(argv[0]); n=strlen(suffix);
    if (length != base+34+n || length >= 192 || strcmp(argv[0]+length-n,suffix) ||
        (strncmp(argv[0],"ms0:/PSP/GAME/PROXUPDATE/",base) && strncmp(argv[0],"ef0:/PSP/GAME/PROXUPDATE/",base))) return -1;
    component=argv[0]+base;
    if (strncmp(component,".stage-pro-x-",13) || !prox_ota_hex(component+13,12) ||
        component[25] != '-' || !prox_ota_hex(component+26,8)) return -1;
    memcpy(root,argv[0],length-n); root[length-n]=0;
    return 1;
}
#endif
