#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "prox_completion_preview.h"
#include "native_preview_stream.h"
#include "native_error_text.h"
/* Execute the generated bridge's supported MIPS subset, including delay slots.
 * Model destructive syscall argument/GP clobbers and both return paths. */
static void bridge_execution(int direct,int navigation)
{
    const unsigned int base=0x08900000,original=0x08812340,caller=0x0888fff0;
    unsigned int code[34],r[32]={0},stack[32]={0},pc=base,pending=0,steps=0,notices=0,keys=0;
    if(navigation) assert(!prox_native_back_code(code,34,original,9,base));
    else assert(!prox_native_notice_code(code,34,original,7,8,base));
    r[29]=0x0a001000;r[31]=caller;r[28]=0x12345678;r[4]=0x08b12340;r[5]=0x08b23450;
    if(navigation) r[4]=1;
    while(pc!=original && pc!=caller) {
        unsigned int w,op,a,b,d,old=pending;int imm;
        assert(++steps<100 && pc>=base && pc<base+sizeof(code) && !(pc&3));
        w=code[(pc-base)/4];op=w>>26;a=(w>>21)&31;b=(w>>16)&31;d=(w>>11)&31;imm=(short)w;pending=0;
        if(!w) { }
        else if(op==9) r[b]=r[a]+imm;
        else if(op==0x2b || op==0x23) {
            unsigned int address=r[a]+imm;assert(address>=0x0a000fc0 && address<0x0a001040 && !(address&3));
            if(op==0x2b) stack[(address-0x0a000fc0)/4]=r[b];else r[b]=stack[(address-0x0a000fc0)/4];
        } else if(op==3) { r[31]=pc+8;pending=((pc+4)&0xf0000000)|((w&0x03ffffff)<<2); }
        else if(op==4) { if(r[a]==r[b]) pending=pc+4+imm*4; }
        else if(op==15) r[b]=(w&0xffff)<<16;
        else if(op==13) r[b]=r[a]|(w&0xffff);
        else if(!op && (w&63)==0x21) r[d]=r[a]+r[b];
        else if(!op && (w&63)==8) pending=r[a];
        else if(!op && (w&63)==12) {
            unsigned int call=(w>>6)&0xfffff;
            assert(r[4]==(navigation?1u:0x08b23450u));
            if(navigation) {
                unsigned int home;
                assert(call==9);++notices;r[2]=direct?0:1;
                for(home=0;home<4;++home) stack[(r[29]-0x0a000fc0)/4+home]=0xdeadbeef;
            }
            else if(call==8) { ++notices;r[2]=direct?0x08a00000:0; }
            else { assert(call==7 && !direct);++keys;r[2]=0x08889900; }
            r[4]=0xdeadbeef;r[5]=0xbad0bad0;r[28]=0xdeadbabe;
        } else assert(0);
        r[0]=0;pc=old?old:pc+4;
    }
    assert(notices==1 && keys==(unsigned int)(!navigation && !direct) && r[29]==0x0a001000 && r[28]==0x12345678 && r[31]==caller);
    if(navigation) assert(pc==original && r[4]==(unsigned int)!direct);
    else if(direct) assert(pc==caller && r[2]==0x08a00000);
    else assert(pc==original && r[4]==0x08b12340 && r[5]==0x08889900);
}
int main(void)
{
    ProxCompletionState state={0};unsigned short text[]={0x4e00,0x00e9,0},out[120]={0};
    ProxPreviewStream stream={0};unsigned int serial=0,code[36],before[36];int fd,n;
    assert(prox_completion_arm(&state,0,out,text,3)<0);
    assert(!prox_completion_arm(&state,77,out,text,3) && !memcmp(out,text,sizeof(text)));
    assert(!prox_completion_select(&state,77,"msgupdate_downloaded_system_update"));
    assert(prox_completion_back(&state,77,1)==1);
    assert(prox_completion_finish(&state,76,9)<0 && prox_completion_finish(&state,77,0)<0);
    assert(!prox_completion_finish(&state,77,9));
    assert(prox_completion_finish(&state,77,9)<0 && prox_completion_poll(&state,77,8)<0);
    assert(!prox_completion_select(&state,76,"msgupdate_downloaded_system_update"));
    assert(!prox_completion_select(&state,77,"msgupdate_error_start"));
    assert(!prox_completion_poll(&state,77,9));
    assert(prox_completion_select(&state,77,"msgupdate_downloaded_system_update"));
    assert(prox_completion_poll(&state,77,9)==1);
    assert(!prox_completion_back(&state,77,1) && !prox_completion_back(&state,77,0));
    assert(prox_completion_back(&state,76,1)==1);
    {
        unsigned int button=0x4000;
        {
            state.released=state.restart=0;
            prox_completion_buttons(&state,76,0);prox_completion_buttons(&state,77,button);
            assert(prox_completion_poll(&state,77,9)==1); /* Held consent cannot restart. */
            prox_completion_buttons(&state,77,0x10); /* Other keys release X/O but do not restart. */
            assert(prox_completion_poll(&state,77,9)==1);
            prox_completion_buttons(&state,77,0x2000);
            assert(prox_completion_poll(&state,77,9)==1); /* Circle never restarts. */
            prox_completion_buttons(&state,77,button);
            assert(prox_completion_poll(&state,77,9)==2);
        }
        state.last_poll=0xfffffff0u;
        assert(prox_completion_health(&state,77,0x1000)==2);
        assert(prox_completion_health(&state,77,0xfffffff0u+PROX_COMPLETION_LIVE_US)<0);
        state.last_poll=120000000;
        assert(prox_completion_health(&state,77,120000001)==2); /* No total user deadline. */
        state.failed=1;
        assert(prox_completion_back(&state,77,1)==1);
        assert(prox_completion_health(&state,77,120000001)<0 && prox_completion_poll(&state,77,9)<0);
    }
    text[1]='\n';assert(prox_completion_arm(&state,78,out,text,3)<0);text[1]=0x00e9;
    assert(!prox_completion_arm(&state,78,out,text,3) && prox_completion_poll(&state,77,9)<0);
    prox_preview_stream_reset(&stream);assert(!prox_preview_stream_offer(&stream,131072));fd=prox_preview_stream_open(&stream,7);
    assert(prox_preview_stream_finish(&stream,7,32768,stream.serial,1)<0);
    while((n=prox_preview_stream_peek(&stream,7,32768,&serial))>0) {
        assert(prox_preview_stream_read(&stream,7,serial,n)==n);
        assert(prox_preview_stream_write(&stream,7,fd,n)==n);
    }
    assert(prox_preview_stream_finish(&stream,7,32768,serial,0)<0);
    assert(prox_preview_stream_finish(&stream,8,32768,serial,1)<0);
    assert(prox_preview_stream_finish(&stream,7,32767,serial,1)<0);
    assert(prox_preview_stream_finish(&stream,7,32768,serial+1,1)<0);
    assert(prox_preview_stream_finish(&stream,7,32768,serial,1)==32768);
    assert(prox_preview_stream_finish(&stream,7,32768,serial,1)<0);
    assert(prox_preview_stream_write(&stream,7,fd,32768)==32768 && stream.written==stream.total);
    memset(code,0xa5,sizeof(code));memcpy(before,code,sizeof(code));
    assert(prox_native_notice_code(code+1,33,0x08812340,7,8,0x08900000)<0 && !memcmp(code,before,sizeof(code)));
    assert(prox_native_notice_code(code+1,34,0x88012340,7,8,0x08900000)<0);
    assert(prox_native_notice_code(code+1,34,0x08812340,7,0x100000,0x08900000)<0);
    assert(!memcmp(code,before,sizeof(code)));
    assert(!prox_native_notice_code(code+1,34,0x08812340,7,8,0x08900000));
    assert(code[0]==0xa5a5a5a5 && code[35]==0xa5a5a5a5);
    assert(code[7]==(0x0c000000u|(0x08900078u>>2)) && code[9]==0x10400005);
    assert(code[16]==(0x0c000000u|(0x08900070u>>2)));
    assert(code[30]==((7u<<6)|12u) && code[32]==((8u<<6)|12u));
    assert(code[33]==0x03e00008 && code[34]==0x00001021);
    memset(code,0xa5,sizeof(code));memcpy(before,code,sizeof(code));
    assert(prox_native_back_code(code+1,13,0x08812340,9,0x08900000)<0);
    assert(prox_native_back_code(code+1,14,0x88012340,9,0x08900000)<0 && !memcmp(code,before,sizeof(code)));
    assert(!prox_native_back_code(code+1,14,0x08812340,9,0x08900000));
    assert(code[0]==0xa5a5a5a5 && code[15]==0xa5a5a5a5);
    bridge_execution(0,0);bridge_execution(1,0);bridge_execution(0,1);bridge_execution(1,1);
    puts("PASS: bounded completion, fresh Cross only, Circle ignored, Back hidden only for current completion, and MIPS text/boolean bridges preserve stack/RA/GP/native arguments");
}
