/* Synthetic RCO fixture, contains no Sony data or translated resource copies. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_completion_text.h"
static unsigned char fixture[2048];
static void word(unsigned int p,unsigned int v)
{ fixture[p]=v;fixture[p+1]=v>>8;fixture[p+2]=v>>16;fixture[p+3]=v>>24; }
static void half(unsigned int p,unsigned int v) { fixture[p]=v;fixture[p+1]=v>>8; }
static FILE *make(void)
{
    FILE *file=tmpfile();assert(file);assert(fwrite(fixture,1,sizeof(fixture),file)==sizeof(fixture));return file;
}
int main(void)
{
    unsigned int i,table=164,start=table+40,txt=start+12*60,label=txt+120;
    unsigned short out[120];FILE *file;
    memcpy(fixture,"\0PRF",4);word(4,0x71);word(0x18,table);
    word(0x38,txt);word(0x3c,120);word(0x40,label);word(0x44,32);
    word(table,0x300);word(table+16,12);
    strcpy((char *)fixture+label,"msgupdater_updatecomplete_ms_1");
    for(i=0;i<12;++i) {
        unsigned int e=start+i*60;
        word(e,0x301);word(e+8,40);word(e+20,60);half(e+40,i);half(e+42,1);word(e+44,1);
        word(e+48,0);word(e+52,10);word(e+56,i*10);
        half(txt+i*10,'A'+i);half(txt+i*10+2,'!');half(txt+i*10+4,'\n');half(txt+i*10+6,'X');
    }
    file=make();
    for(i=0;i<12;++i) { assert(prox_completion_read_text(file,i,out,120)==3);assert(out[0]=='A'+i && out[1]=='!' && !out[2]); }
    assert(prox_completion_read_text(file,12,out,120)<0);assert(prox_completion_read_text(file,0,out,2)<0);fclose(file);
    word(12,0x20);file=make();assert(prox_completion_read_text(file,0,out,120)<0);fclose(file);word(12,0);
    word(start+56,0xffffffff);file=make();assert(prox_completion_read_text(file,0,out,120)<0);fclose(file);word(start+56,0);
    word(start+20,1);file=make();assert(prox_completion_read_text(file,1,out,120)<0);fclose(file);word(start+20,60);
    fixture[label]='x';file=make();assert(prox_completion_read_text(file,0,out,120)<0);fclose(file);
    puts("PASS: bounded original-resource reader selects all 12 synthetic locales, first line only, and rejects packing/traversal/index/step/capacity/missing-key errors");
}
