/* Fixed PRO paths; verified backups and copies. Original 6.61 flashfat's
 * IoRename callback always returns 0x80010086 (ENOTSUP). No rename fallback,
 * driver patch, raw-sector write or unverified destructive operation. */
#include "native_flash.h"
#include "stage.h"
#include "config.h"
#include "transport.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <mbedtls/sha256.h>
#ifdef PROX_NATIVE_FLASH_TEST
size_t prox_flash_test_write(const void *,size_t,size_t,FILE *);
FILE *prox_flash_test_open(const char *,const char *);
#define fwrite prox_flash_test_write
#define fopen prox_flash_test_open
#endif
static const char *const paths[]={PATH_SYSTEMCTRL,PATH_VSHCTRL,PATH_GALAXY,PATH_STARGATE,PATH_MARCH33,
    PATH_INFERNO,PATH_USBDEVICE,PATH_POPCORN,PATH_SATELITE,PATH_RECOVERY};
const char *prox_native_flash_path(unsigned int id) { return id<10?paths[id]:NULL; }
static int path(char out[256],unsigned int id,const char *suffix)
{ int n=snprintf(out,256,"%s%s",paths[id],suffix);return n<0 || n>=256?-1:0; }
static int exists(const char *p)
{
    struct stat st;int error;errno=0;
    if(!stat(p,&st)) {
        if(S_ISREG(st.st_mode))return 1;
        prox_log("FLASH_FAIL operation=stat-type path=%s mode=0x%08x",p,(unsigned int)st.st_mode);return -1;
    }
    error=errno;if(error==ENOENT)return 0;
    prox_log("FLASH_FAIL operation=stat path=%s errno=%d",p,error);return -1;
}
static int checked(const char *p,const ProxNativePart *part,const char *operation)
{
    int rc,error;struct stat st;errno=0;rc=prox_file_check(p,part->size,part->sha256,0);error=errno;
    if(rc) {
        int meta=stat(p,&st);
        prox_log("FLASH_FAIL operation=%s path=%s errno=%d expected_size=%u actual_size=%lld",operation,p,error,part->size,meta?-1LL:(long long)st.st_size);
    }
    return rc;
}
static int remove_file(const char *p,const char *operation)
{
    int rc,error;errno=0;rc=remove(p);error=errno;
    if(rc)prox_log("FLASH_FAIL operation=%s path=%s errno=%d",operation,p,error);
    return rc;
}
/* Snapshot the exact original bytes, never substitute the target hash. */
static int fingerprint(const char *p,ProxNativePart *part)
{
    FILE *f;struct stat st;unsigned char buffer[4096],digest[32];size_t n;unsigned int total=0,i;
    mbedtls_sha256_context ctx;int rc=-1;
    if(stat(p,&st) || !S_ISREG(st.st_mode) || st.st_size<=0 || (unsigned long long)st.st_size>PROX_UPDATE_MAX)return -1;
    f=fopen(p,"rb");if(!f)return -1;
    mbedtls_sha256_init(&ctx);
    if(mbedtls_sha256_starts(&ctx,0))goto done;
    while((n=fread(buffer,1,sizeof(buffer),f))!=0) {
        if(n>(unsigned int)st.st_size-total || mbedtls_sha256_update(&ctx,buffer,n))goto done;
        total+=(unsigned int)n;
    }
    if(ferror(f) || total!=(unsigned int)st.st_size || mbedtls_sha256_finish(&ctx,digest))goto done;
    part->size=total;for(i=0;i<32;++i)snprintf(part->sha256+2*i,3,"%02x",digest[i]);rc=0;
done:
    mbedtls_sha256_free(&ctx);if(fclose(f))rc=-1;
    if(rc)prox_log("FLASH_FAIL operation=fingerprint-original path=%s errno=%d",p,errno);
    return rc;
}
/* Caller records ownership BEFORE this may create/truncate a destination.
 * Check the complete source before opening the destination, then both again. */
static int copy_checked(const char *from,const char *to,const ProxNativePart *part,const char *operation)
{
    FILE *in,*out;unsigned char buffer[4096];size_t n;unsigned int total=0;int failed=0,error;
    if(checked(from,part,"verify-copy-source"))return -1;
    errno=0;in=fopen(from,"rb");if(!in){prox_log("FLASH_FAIL operation=open-source path=%s errno=%d",from,errno);return -1;}
    errno=0;out=fopen(to,"wb");if(!out){error=errno;fclose(in);prox_log("FLASH_FAIL operation=%s-open path=%s errno=%d",operation,to,error);return -1;}
    prox_log("FLASH_COPY operation=%s source=%s destination=%s expected=%u",operation,from,to,part->size);
    while((n=fread(buffer,1,sizeof(buffer),in))!=0) {
        size_t written;
        if(n>part->size-total){prox_log("FLASH_FAIL operation=%s-source-size path=%s",operation,from);failed=1;break;}
        errno=0;written=fwrite(buffer,1,n,out);
        if(written!=n){prox_log("FLASH_FAIL operation=%s-write path=%s errno=%d wanted=%u written=%u",operation,to,errno,(unsigned int)n,(unsigned int)written);failed=1;break;}
        total+=(unsigned int)n;
    }
    if(total!=part->size)failed=1;
    if(ferror(in)){prox_log("FLASH_FAIL operation=%s-read path=%s errno=%d",operation,from,errno);failed=1;}
    errno=0;if(fclose(in)){prox_log("FLASH_FAIL operation=%s-close-source path=%s errno=%d",operation,from,errno);failed=1;}
    errno=0;if(fclose(out)){prox_log("FLASH_FAIL operation=%s-close-target path=%s errno=%d",operation,to,errno);failed=1;}
    if(failed || checked(to,part,"verify-copy-destination") || checked(from,part,"verify-copy-source-after"))return -1;
    return 0;
}
int prox_native_flash_prepare(const ProxNativeInstallJob *job,ProxNativeFlash *t)
{
    unsigned int i;char a[256],b[256];
    if(!job || !t || t->prepared || t->changed || t->originals || t->backups || t->identical)return -1;
    prox_log("FLASH_PREPARE checked-copy strategy; original flashfat rename unsupported; all backups before replacement");
    for(i=0;i<10;++i) {
        int fresh,backup,original;
        if(path(a,i,".prox-new") || path(b,i,".prox-old"))return -1;
        fresh=exists(a);backup=exists(b);original=exists(paths[i]);
        prox_log("FLASH_PREFLIGHT part=%u original=%d new=%d old=%d path=%s",i,original,fresh,backup,paths[i]);
        if(fresh!=0 || backup!=0 || original<0)return -1;
    }
    for(i=0;i<10;++i) {
        unsigned int bit=1u<<i;
        if(!prox_file_check(paths[i],job->parts[i].size,job->parts[i].sha256,0)) {
            t->identical|=bit;prox_log("FLASH_SKIP identical part=%u path=%s",i,paths[i]);continue;
        }
        if(snprintf(a,sizeof(a),"%s/native/%02u.prx",job->root,i)>=(int)sizeof(a) || path(b,i,".prox-new"))goto failure;
        t->prepared|=bit;
        if(copy_checked(a,b,&job->parts[i],"prepare-target"))goto failure;
    }
    for(i=0;i<10;++i)if(!(t->identical&(1u<<i))) {
        int original=exists(paths[i]);unsigned int bit=1u<<i;
        if(original<0)goto failure;
        if(!original)continue;
        if(fingerprint(paths[i],&t->before[i]) || path(b,i,".prox-old"))goto failure;
        t->backups|=bit;
        if(copy_checked(paths[i],b,&t->before[i],"backup-original"))goto failure;
        t->originals|=bit;
        prox_log("FLASH_BACKUP verified part=%u size=%u sha256=%s path=%s",i,t->before[i].size,t->before[i].sha256,b);
    }
    return 0;
failure:
    (void)prox_native_flash_rollback(job,t);return -1;
}
int prox_native_flash_commit(const ProxNativeInstallJob *job,ProxNativeFlash *t)
{
    unsigned int i;char a[256],b[256];
    if(!job || !t || t->changed || t->backups!=t->originals)return -1;
    /* Revalidate ALL sources and original backups after checked flush/reopen,
     * before first destructive open, including originally absent paths. */
    for(i=0;i<10;++i) {
        unsigned int bit=1u<<i;
        if(t->identical&bit){if(checked(paths[i],&job->parts[i],"verify-identical"))return -1;continue;}
        if(!(t->prepared&bit) || path(a,i,".prox-new") || path(b,i,".prox-old") || checked(a,&job->parts[i],"verify-prepared"))return -1;
        if(t->originals&bit) {
            if(checked(b,&t->before[i],"verify-backup") || checked(paths[i],&t->before[i],"verify-original-unchanged"))return -1;
        } else if(exists(paths[i])!=0 || exists(b)!=0)return -1;
    }
    for(i=0;i<10;++i)if(!(t->identical&(1u<<i))) {
        if(path(a,i,".prox-new"))return -1;
        /* fopen may truncate before a later runtime allocation fails. */
        t->changed|=1u<<i;
        if(copy_checked(a,paths[i],&job->parts[i],"replace-module"))return -1;
    }
    return 0;
}
int prox_native_flash_rollback(const ProxNativeInstallJob *job,ProxNativeFlash *t)
{
    int i,failed=0;char a[256],b[256];
    if(!job || !t)return -1;
    for(i=9;i>=0;--i) {
        unsigned int bit=1u<<i;
        if(path(a,(unsigned int)i,".prox-new") || path(b,(unsigned int)i,".prox-old")){failed=1;continue;}
        if(t->changed&bit) {
            if(t->originals&bit) {
                if(copy_checked(b,paths[i],&t->before[i],"rollback-original")){failed=1;continue;}
            } else {
                int present=exists(paths[i]);
                if(present<0 || (present && remove_file(paths[i],"rollback-created-module"))){failed=1;continue;}
            }
            t->changed&=~bit;
        }
        if(t->backups&bit) {
            int present=exists(b);
            if(present<0 || (present && remove_file(b,"rollback-remove-backup"))){failed=1;continue;}
            t->backups&=~bit;t->originals&=~bit;
        }
        if(t->prepared&bit) {
            int present=exists(a);
            if(present<0 || (present && remove_file(a,"rollback-remove-temp")))failed=1;
            else t->prepared&=~bit;
        }
    }
    prox_log("FLASH_ROLLBACK result=%d prepared=0x%03x changed=0x%03x originals=0x%03x backups=0x%03x",failed?-1:0,t->prepared,t->changed,t->originals,t->backups);
    return failed?-1:0;
}
int prox_native_flash_finalize(const ProxNativeInstallJob *job,ProxNativeFlash *t)
{
    unsigned int i;char p[256];int failed=0;
    if(!job || !t)return -1;
    for(i=0;i<10;++i)if(checked(paths[i],&job->parts[i],"verify-before-finalize"))return -1;
    for(i=0;i<10;++i) {
        unsigned int bit=1u<<i;
        if(t->backups&bit) {
            if(path(p,i,".prox-old") || remove_file(p,"remove-backup"))failed=1;
            else {t->backups&=~bit;t->originals&=~bit;}
        }
        if(t->prepared&bit) {
            if(path(p,i,".prox-new") || remove_file(p,"remove-prepared"))failed=1;
            else t->prepared&=~bit;
        }
    }
    return failed?-1:0;
}
