#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "native_error_text.h"
#include "prox_native_query.h"
int main(void)
{
    const char *network="msgupdate_error_download_cannot_connect",*storage="msgshare_error_notplay_ms",*generic="msgupdate_error_start";
    unsigned int guarded[20],before[20]; size_t i;
    assert(prox_native_error_select(PROX_NQ_CAUSE_STORAGE,network,storage,generic)==storage);
    assert(prox_native_error_select(PROX_NQ_CAUSE_POWER,network,storage,generic)==generic);
    assert(prox_native_error_select(PROX_NQ_CAUSE_CLIENT,"msgupdate_error_connect",storage,generic)==generic);
    assert(prox_native_error_select(PROX_NQ_CAUSE_NONE,network,storage,generic)==network);
    assert(!strcmp(prox_native_error_select(PROX_NQ_CAUSE_POWER,"msgshare_latest_ver",storage,generic),"msgshare_latest_ver"));
    assert(!strcmp(prox_native_error_select(PROX_NQ_CAUSE_STORAGE,"msgupdate_new_found",storage,generic),"msgupdate_new_found"));
    assert(prox_native_error_select(100,network,storage,generic)==network);
    assert(!prox_native_error_select(PROX_NQ_CAUSE_POWER,NULL,storage,generic));
    for(i=0;i<20;++i) guarded[i]=before[i]=0xdeadbeef;
    assert(prox_native_error_code(guarded+1,17,0x08812340,7,0x08900000)<0 && !memcmp(guarded,before,sizeof(guarded)));
    assert(prox_native_error_code(guarded+1,18,0x88012340,7,0x08900000)<0);
    assert(prox_native_error_code(guarded+1,18,0x08812341,7,0x08900000)<0);
    assert(prox_native_error_code(guarded+1,18,0x08812340,0x100000,0x08900000)<0);
    assert(prox_native_error_code(guarded+1,18,0x08812340,7,0x0bfffffc)<0);
    assert(prox_native_error_code(guarded+1,18,0x08812340,7,0x88900000)<0);
    assert(!memcmp(guarded,before,sizeof(guarded)));
    assert(!prox_native_error_code(guarded+1,18,0x08812340,7,0x08900000));
    assert(guarded[0]==0xdeadbeef && guarded[19]==0xdeadbeef);
    assert(guarded[1]==0x27bdffe0 && guarded[2]==0xafbf001c && guarded[3]==0xafbc0018 && guarded[4]==0xafa40014);
    assert(guarded[5]==0x00a02021 && guarded[6]==(0x0c000000u|(0x08900040u>>2)) && guarded[7]==0 && guarded[8]==0x00402821 && guarded[9]==0x8fa40014);
    assert(guarded[10]==0x8fbc0018 && guarded[11]==0x8fbf001c && guarded[12]==0x3c190881 && guarded[13]==0x37392340);
    assert(guarded[14]==0x03200008 && guarded[15]==0x27bd0020 && guarded[16]==0);
    assert(guarded[17]==0x03e00008 && guarded[18]==((7u<<6)|12u));
    puts("PASS: local storage/power/client failures select existing native resources; network/current/offer unchanged; bounded user MIPS tail-call preserves resource/RA/GP");
}
