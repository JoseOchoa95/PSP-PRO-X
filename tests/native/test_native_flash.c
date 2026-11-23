#define _GNU_SOURCE
/* Actual host files and real SHA-256; no PSP device/flash is available. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <mbedtls/sha256.h>
#include "native_flash.h"
static int mode,phase,writes,destructive,failures;
static FILE *output;
static char destination[256],logs[65536];static size_t used;
void prox_log(const char *format,...)
{int n;va_list args;va_start(args,format);n=vsnprintf(logs+used,sizeof(logs)-used,format,args);va_end(args);assert(n>=0 && (size_t)n<sizeof(logs)-used);used+=(size_t)n;}
/* A regression to ANY rename would fail even in the success fixture. */
int rename(const char *a,const char *b)
{(void)a;(void)b;assert(!"6.61 flashfat cannot rename");return -1;}
FILE *prox_flash_test_open(const char *p,const char *flags)
{
    FILE *f=fopen(p,flags);
    if(!strcmp(flags,"wb")) {
        output=f;strcpy(destination,p);
        if(!strstr(p,".prox-")) {
            ++destructive;
            if(phase==1 && mode==6 && !failures){++failures;assert(f && !fclose(f));errno=ENOMEM;return NULL;}
        }
    }
    return f;
}
size_t prox_flash_test_write(const void *p,size_t size,size_t n,FILE *f)
{
    int inject=0;++writes;assert(f==output);
    if(mode==1 && !failures)inject=1;
    if(mode==5 && strstr(destination,".prox-old") && !failures)inject=1;
    if((mode==2 || mode==7) && phase==1 && destructive==2 && !failures)inject=1;
    if(mode==7 && phase==2 && !strstr(destination,".prox-") && failures==1)inject=1;
    if(inject){size_t actual;++failures;assert(n>1);actual=fwrite(p,size,n-1,f);errno=ENOSPC;return actual;}
    return fwrite(p,size,n,f);
}
static void hash_bytes(const unsigned char *data,size_t n,char out[65])
{unsigned char digest[32];unsigned int i;assert(!mbedtls_sha256(data,n,digest,0));for(i=0;i<32;++i)snprintf(out+2*i,3,"%02x",digest[i]);}
int prox_file_check(const char *p,unsigned int size,const char *hash,int pbp)
{
    FILE *f=fopen(p,"rb");unsigned char b[512];char actual[65];size_t n;(void)pbp;
    if(!f)return -1;
    n=fread(b,1,sizeof(b),f);assert(!ferror(f) && !fclose(f));
    hash_bytes(b,n,actual);return n!=size || strcmp(actual,hash)?-1:0;
}
static void dirs(const char *path)
{char copy[256];size_t i;strcpy(copy,path);for(i=0;copy[i];++i)if(copy[i]=='/'){copy[i]=0;if(i)assert(!mkdir(copy,0700)||errno==EEXIST);copy[i]='/';}}
static void fill(const char *path,int c,unsigned int n)
{FILE *f;dirs(path);f=fopen(path,"wb");assert(f);while(n--)assert(fputc(c,f)!=EOF);assert(!fclose(f));}
static void old_file(const char *path)
{FILE *f=fopen(path,"rb");int c,n=0;assert(f);while((c=fgetc(f))!=EOF){assert(c==127);++n;}assert(n==32 && !fclose(f));}
int main(void)
{
    char temp[]="/tmp/prox-native-flash-XXXXXX",cwd[512],p[256];unsigned int i;ProxNativeInstallJob job={0};
    static const char *const expected[]={"flash0:/kd/_systemctrl.prx","flash0:/kd/_vshctrl.prx","flash0:/kd/_galaxy.prx",
      "flash0:/kd/_stargate.prx","flash0:/kd/_march33.prx","flash0:/kd/_inferno.prx","flash0:/kd/_usbdevice.prx",
      "flash0:/kd/_popcorn.prx","flash0:/vsh/module/_satelite.prx","flash0:/vsh/module/_recovery.prx"};
    assert(getcwd(cwd,sizeof(cwd)) && mkdtemp(temp) && !chdir(temp));
    strcpy(job.root,"ms0:/PSP/GAME/PROXUPDATE/.stage-pro-x-123456789abc-00112233");
    for(mode=0;mode<11;++mode) {
        ProxNativeFlash state={0};char run[16];sprintf(run,"run-%d",mode);assert(!mkdir(run,0700) && !chdir(run));
        writes=destructive=failures=phase=0;used=0;logs[0]=0;
        for(i=0;i<10;++i) {
            unsigned char b[32];assert(!strcmp(prox_native_flash_path(i),expected[i]));
            job.parts[i].size=32;memset(b,(int)i+1,sizeof(b));hash_bytes(b,sizeof(b),job.parts[i].sha256);
            snprintf(p,sizeof(p),"%s/native/%02u.prx",job.root,i);fill(p,(int)i+1,32);
            dirs(expected[i]);if(mode!=9)fill(expected[i],mode==3?(int)i+1:127,32);
        }
        if(mode==4){snprintf(p,sizeof(p),"%s.prox-old",expected[0]);fill(p,99,32);}
        if(mode==1 || mode==4 || mode==5) {
            assert(prox_native_flash_prepare(&job,&state)<0 && !state.changed && !destructive);
            for(i=0;i<10;++i)old_file(expected[i]);
            if(mode==4)assert(!writes && strstr(logs,"new=0 old=1"));
            else assert(strstr(logs,"errno=28") && strstr(logs,"FLASH_ROLLBACK result=0"));
        } else {
            { int result=prox_native_flash_prepare(&job,&state);
              if(result || destructive)fprintf(stderr,"mode=%d result=%d destructive=%d\n%s\n",mode,result,destructive,logs);
              assert(!result && !destructive); }
            if(mode!=9 && mode!=3)for(i=0;i<10;++i){old_file(expected[i]);snprintf(p,sizeof(p),"%s.prox-old",expected[i]);old_file(p);}
            if(mode==8){snprintf(p,sizeof(p),"%s.prox-old",expected[9]);fill(p,99,32);}
            if(mode==10){snprintf(p,sizeof(p),"%s.prox-new",expected[9]);fill(p,99,32);}
            phase=1;
            if(mode==2 || mode==6 || mode==7 || mode==8 || mode==10) {
                assert(prox_native_flash_commit(&job,&state)<0);
                if(mode==8 || mode==10)assert(!destructive);
                phase=2;
                if(mode==7) {
                    assert(prox_native_flash_rollback(&job,&state)<0 && state.changed && state.originals && state.backups);
                    snprintf(p,sizeof(p),"%s.prox-old",expected[1]);old_file(p);
                    phase=3;assert(!prox_native_flash_rollback(&job,&state));
                } else assert(!prox_native_flash_rollback(&job,&state));
                for(i=0;i<10;++i)old_file(expected[i]);
            } else {
                assert(!prox_native_flash_commit(&job,&state));
                for(i=0;i<10;++i)assert(!prox_file_check(expected[i],32,job.parts[i].sha256,0));
                assert(!prox_native_flash_finalize(&job,&state) && !state.originals && !state.backups && !state.prepared);
                if(mode==3)assert(!writes && !state.changed);
            }
        }
        assert(!chdir(".."));
    }
    assert(!chdir(cwd));
    puts("PASS: actual SHA-256 backups/copies with rename prohibited; all backups before original opens; target/backup corruption, ENOSPC, post-truncate open failure, absent originals, unchanged smart-skip, rollback failure retains exact backup and retry restores; no PSP flash");
}
