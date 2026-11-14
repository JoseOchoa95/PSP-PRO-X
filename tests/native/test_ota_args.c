#include "prox_ota_args.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    char root[192];
    char exe[]="ms0:/PSP/GAME/PROXUPDATE/.stage-pro-x-123456789abc-00000001/PROUPDATE/EBOOT.PBP";
    char hash[]="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char *args[]={exe,"--prox-ota-v1",hash,"0.3.0"};
    assert(prox_ota_args(1,args,"/PROUPDATE/EBOOT.PBP",root) < 0);
    {
        char *manual[]={"ms0:/PSP/GAME/PROUPDATE/EBOOT.PBP"};
        assert(prox_ota_args(1,manual,"/PROUPDATE/EBOOT.PBP",root) == 0);
    }
    assert(prox_ota_args(4,args,"/PROUPDATE/EBOOT.PBP",root) == 1);
    assert(!strcmp(root,"ms0:/PSP/GAME/PROXUPDATE/.stage-pro-x-123456789abc-00000001"));
    assert(prox_ota_args(3,args,"/PROUPDATE/EBOOT.PBP",root) < 0);
    assert(prox_ota_args(4,args,"/PROUPDATE/installer.prx",root) < 0);
    hash[2]='g'; assert(prox_ota_args(4,args,"/PROUPDATE/EBOOT.PBP",root) < 0); hash[2]='2';
    exe[2]='1'; assert(prox_ota_args(4,args,"/PROUPDATE/EBOOT.PBP",root) < 0); exe[2]='0';
    args[3]="0.3.0-beta"; assert(prox_ota_args(4,args,"/PROUPDATE/EBOOT.PBP",root) < 0);
    puts("PASS: private launcher/installer argument framing, source snapshot, hash and confined stage paths");
    return 0;
}
