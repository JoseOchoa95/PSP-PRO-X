#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mbedtls/sha256.h>
#include "resident_loader_shim.h"
#include "native_dedicated_cleanup.h"
#include "native_mount_access.h"
#include "prox_native_flash_paths.h"
#include "prox_resident_image.h"

static unsigned int mounts, removals, fail_after;
static int active, fail_restore;

void mbedtls_platform_zeroize(void *buffer,size_t size) { memset(buffer,0,size); }
unsigned int sceKernelDevkitVersion(void) { return 0x06060110u; }
int sceKernelGetModel(void) { return 1; }
int sceIoGetstat(const char *path,SceIoStat *out)
{
    struct stat status;
    if(stat(path,&status))return errno==ENOENT ? (int)0x80010002 : -1;
    out->st_mode=S_ISREG(status.st_mode);
    out->st_size=status.st_size;
    return 0;
}
int sceIoOpen(const char *path,int flags,int mode)
{
    assert(flags==PSP_O_RDONLY && !mode);
    return open(path,O_RDONLY);
}
int sceIoRead(int fd,void *out,unsigned int size) { return (int)read(fd,out,size); }
int sceIoClose(int fd) { return close(fd); }
int sceIoRemove(const char *path)
{
    size_t length=strlen(path);
    assert(active && length>9 && (!strcmp(path+length-9,".prox-old") || !strcmp(path+length-9,".prox-new")));
    if(fail_after && removals==fail_after)return -1;
    assert(!unlink(path));
    ++removals;
    return 0;
}
int prox_native_mount_begin(ProxMountReport *report)
{
    assert(!active);
    report->active=active=1;
    ++mounts;
    return 0;
}
int prox_native_mount_end(ProxMountReport *report)
{
    assert(active);
    if(fail_restore)return -1;
    report->active=active=0;
    report->restored=1;
    return 0;
}

static void put(const char *path,const void *bytes,size_t size)
{
    FILE *out=fopen(path,"wb");
    assert(out && fwrite(bytes,1,size,out)==size && !fclose(out));
}

static ProxNativePart part(const void *bytes,unsigned int size)
{
    unsigned char hash[32];
    ProxNativePart out={0};
    out.size=size;
    assert(!mbedtls_sha256(bytes,size,hash,0));
    for(unsigned int i=0;i<sizeof(hash);++i)snprintf(out.sha256+2*i,3,"%02x",(unsigned int)hash[i]);
    return out;
}

int main(void)
{
    char directory[]="/tmp/prox-boot-cleanup-XXXXXX";
    char backup[96], prepared[96], worker_stage[96];
    const char old[]="previous", current[]="current!", foreign[]="unrelated";
    const char foreign_path[]="flash0:/kd/unrelated.prox-old";
    unsigned char worker[70]={0x7f,'E','L','F',1,1};
    ProxNativeCleanupMessage job={0};
    assert(mkdtemp(directory) && !chdir(directory));
    assert(!mkdir("flash0:",0700) && !mkdir("flash0:/kd",0700) &&
           !mkdir("flash0:/vsh",0700) && !mkdir("flash0:/vsh/module",0700));
    job.model=1;
    strcpy(job.build_id,PROX_RESIDENT_BUILD_ID);
    job.prepared=job.backups=job.worker_prepared=1;
    for(unsigned int i=0;i<PROX_NI_FLASH_PARTS;++i) {
        job.installed[i]=part(current,sizeof(current));
        put(prox_native_flash_paths[i],current,sizeof(current));
    }
    for(unsigned int i=6;i<sizeof(worker);++i)worker[i]=(unsigned char)(i-6);
    job.before[0]=part(old,sizeof(old));
    job.worker=part(worker,sizeof(worker));
    assert(!strcmp(job.worker.sha256,PROX_RESIDENT_IMAGE_DIGEST));
    assert(snprintf(backup,sizeof(backup),"%s.prox-old",prox_native_flash_paths[0])>0);
    assert(snprintf(prepared,sizeof(prepared),"%s.prox-new",prox_native_flash_paths[0])>0);
    assert(snprintf(worker_stage,sizeof(worker_stage),"%s.prox-new",PROX_RESIDENT_IMAGE_PATH)>0);
    put(PROX_RESIDENT_IMAGE_PATH,worker,sizeof(worker));
    put(backup,old,sizeof(old));
    put(prepared,current,sizeof(current));
    put(worker_stage,worker,sizeof(worker));
    put(foreign_path,foreign,sizeof(foreign));

    job.build_id[0]='x';
    assert(prox_native_cleanup_flash(&job)==-1 && !mounts && !removals);
    assert(job.diagnostic.phase==PROX_CLEANUP_TARGET && job.diagnostic.check==PROX_CLEANUP_CHECK_RECORD);
    strcpy(job.build_id,PROX_RESIDENT_BUILD_ID);
    put(prox_native_flash_paths[0],old,sizeof(old)-1);
    assert(prox_native_cleanup_flash(&job)==-1 && !mounts && !removals);
    assert(job.diagnostic.phase==PROX_CLEANUP_INSTALLED_PART && job.diagnostic.part==0 &&
           job.diagnostic.check==PROX_CLEANUP_CHECK_SIZE);
    put(prox_native_flash_paths[0],current,sizeof(current));
    put(backup,current,sizeof(current));
    assert(prox_native_cleanup_flash(&job)==-1 && !mounts && !removals);
    assert(job.diagnostic.phase==PROX_CLEANUP_BACKUP && job.diagnostic.part==0 &&
           job.diagnostic.check==PROX_CLEANUP_CHECK_HASH);
    put(backup,old,sizeof(old));

    fail_after=1;
    assert(prox_native_cleanup_flash(&job)==-1 && mounts==1 && removals==1 && !active);
    assert(job.diagnostic.phase==PROX_CLEANUP_BACKUP && job.diagnostic.check==PROX_CLEANUP_CHECK_REMOVE);
    fail_after=0;
    assert(!prox_native_cleanup_flash(&job) && mounts==2 && removals==3 && !active);
    assert(!prox_native_cleanup_flash(&job) && mounts==2 && removals==3);
    assert(access(prepared,F_OK) && access(backup,F_OK) && access(worker_stage,F_OK));
    assert(!access(foreign_path,F_OK) && !access(PROX_RESIDENT_IMAGE_PATH,F_OK));

    put(backup,old,sizeof(old));
    fail_restore=1;
    assert(prox_native_cleanup_flash(&job)==-2 && mounts==3 && removals==4 && active);
    assert(job.diagnostic.phase==PROX_CLEANUP_RESTORE);
    puts("PASS: kernel boot cleanup verifies all installed bytes and every owned sidecar, preserves foreign files, and retries safely");
}
