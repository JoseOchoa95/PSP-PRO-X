/* Exact original 6.61 layout, independently inspected offline and guarded
 * against the owner's live driver. Private diagnostic, model 1 only.
 * The lock is the driver's MUTEX (not a semaphore). GPL-3.0-or-later. */
#ifdef PROX_MOUNT_PLATFORM_TEST
#include "native_mount_platform_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr_kernel.h>
#include <pspsysmem_kernel.h>
#include <string.h>
#include "systemctrl.h"
#endif
#include <string.h>
#include "native_mount_access.h"
#include "native_query_provider.h"
typedef int (*LockMutex)(int,int,SceUInt *);
typedef int (*UnlockMutex)(int,int);
static LockMutex lock_mutex;
static UnlockMutex unlock_mutex;
static ProxMountLease mount_lease;
static void checkpoint(const char *message,int result)
{
    /* Called only before acquiring, or after releasing, the FAT mutex.
     * Synchronize only removable-card evidence, never flash0 data. */
    prox_native_query_event(message,result);(void)sceIoSync("ms0:",0);
}
static int kernel(unsigned int a,unsigned int n)
{ return !(a&3u) && a>=0x88000000u && n<=0x400000u && a<=0x88400000u-n; }
static int get(unsigned int a,void *out,unsigned int n)
{ if(!kernel(a,n))return -1;memcpy(out,(const void *)a,n);return 0; }
static unsigned int pair(unsigned int a,unsigned int b)
{ return (a<<16)+(short)(b&0xffffu); }
static int call(unsigned int at,unsigned int destination)
{
    unsigned int word;if(get(at,&word,4))return 0;
    return (word&0xfc000000u)==0x0c000000u &&
           (((at+4)&0xf0000000u)|((word&0x03ffffffu)<<2))==destination;
}
static int view(ProxMountView *out)
{
    unsigned int d[5],f[22],base,v[4],a[2],b[2],count,table,mutex_address;int mutex;
    unsigned int driver=(unsigned int)sctrlHENFindDriver("flashfat");
    if(sceKernelDevkitVersion()!=0x06060110u || sceKernelGetModel()!=1 ||
       get(driver,d,sizeof(d)) || d[1]!=0x001e0010u || get(d[4],f,sizeof(f)) || f[19]<0x88002774u)return -1;
    base=f[19]-0x2774u;
    if(!kernel(base,0x10080u) || driver!=base+0xb74cu || d[4]!=base+0xb6f4u ||
       f[18]!=base+0x2530u || f[20]!=base+0x286cu)return -1;
    if(get(base+0x9004u,v,12) || v[0]!=0x3c030606u || v[1]!=0x03e00008u || v[2]!=0x34620110u ||
       get(base+0x5bb0u,v,16) || v[0]!=0x8e0c0008u || (v[1]&0xffff0000u)!=0x3c0d0000u ||
       v[2]!=0x1d80ffeeu || v[3]!=0x35a50018u)return -1;
    if(get(base+0x5b58u,a,8) || get(base+0x5b8cu,b,8) ||
       (a[0]&0xffff0000u)!=0x3c020000u || (a[1]&0xffff0000u)!=0x8c450000u ||
       (b[0]&0xffff0000u)!=0x3c0a0000u || (b[1]&0xffff0000u)!=0x8d480000u ||
       pair(a[0],a[1])!=base+0xbad4u || pair(b[0],b[1])!=base+0xbad8u ||
       get(base+0xbad4u,&count,4) || !count || count>16 ||
       get(base+0xbad8u,&table,4) || !kernel(table,count*628u))return -1;
    if(get(base+0x277cu,a,4) || get(base+0x27ccu,a+1,4) ||
       (a[0]&0xffff0000u)!=0x3c140000u || (a[1]&0xffff0000u)!=0x8e910000u ||
       (mutex_address=pair(a[0],a[1]))!=base+0xb7c4u ||
       !call(base+0x27d8u,base+0xaf80u) || !call(base+0x2820u,base+0xaf50u) ||
       get(mutex_address,&mutex,4) || mutex<0)return -1;
    if(get(base+0x573cu,a,4) || a[0]!=0x3c0d0400u ||
       get(base+0x5744u,a,4) || a[0]!=0x018d8025u || !call(base+0x5750u,base+0xae60u))return -1;
    /* Original global-sync loop calls this per-volume flush under the same
     * FAT mutex. Guard its parameter, dirty-bit handling and error branch. */
    if(!call(base+0x5dcu,base+0xd3cu) || get(base+0xd3cu,v,12) ||
       v[0]!=0x27bdfff0u || v[1]!=0xafb10004u || v[2]!=0x00808821u ||
       get(base+0xd54u,a,8) || a[0]!=0x8c860000u || a[1]!=0x30c40004u ||
       get(base+0xdb8u,a,4) || a[0]!=0x0440000bu ||
       get(base+0xddcu,a,8) || a[0]!=0x7c061084u || a[1]!=0xae260000u)return -1;
    if(get(table,v,12))return -1;
    memset(out,0,sizeof(*out));out->base=base;out->table=table;out->mutex_address=mutex_address;
    out->mutex=mutex;out->flags=v[0];out->fd=(int)v[1];out->references=v[2];return 0;
}
static int open_owned(void)
{
    int fd;checkpoint("mount access r2: guarded view PASS; before owned raw open",0);
    fd=sceIoOpen("lflash0:0,0",0x04000003,0);
    checkpoint("mount access r2: owned raw open returned",fd);return fd;
}
static int lock(int uid)
{
    int rc;SceUInt timeout=1000000u;
    checkpoint("mount access r2: before original FAT mutex",uid);
    rc=lock_mutex(uid,1,&timeout);
    if(rc<0)checkpoint("mount access r2: original FAT mutex rejected",rc);
    return rc;
}
static int unlock(int uid)
{ int rc=unlock_mutex(uid,1);if(rc>=0)checkpoint("mount access r2: original FAT mutex released",rc);return rc; }
static int64_t seek(int fd,int64_t position,int whence)
{ return sceIoLseek(fd,position,whence); }
static int set_fd(unsigned int table,int fd)
{ if(!kernel(table,12))return -1;*(volatile int *)(table+4)=fd;return 0; }
static int set_flags(unsigned int table,unsigned int flags)
{ if(!kernel(table,12))return -1;*(volatile unsigned int *)table=flags;return 0; }
static int close_owned(int fd){return sceIoClose(fd);}
static const ProxMountOps ops={view,open_owned,lock,unlock,seek,set_fd,set_flags,close_owned};
static int resolve(void)
{
    /* This pinned SDK has no kernel mutex stubs. Resolve the exact 6.61 NIDs
     * observed in the original FAT driver's ThreadManForKernel imports. */
    lock_mutex=(LockMutex)(uintptr_t)sctrlHENFindFunction("sceThreadManager","ThreadManForKernel",0xb011b11fu);
    unlock_mutex=(UnlockMutex)(uintptr_t)sctrlHENFindFunction("sceThreadManager","ThreadManForKernel",0x6b30100fu);
#ifdef PROX_MOUNT_PLATFORM_TEST
    if(!lock_mutex || !unlock_mutex) {
#else
    if(!kernel((unsigned int)lock_mutex,4) || !kernel((unsigned int)unlock_mutex,4)) {
#endif
        return -1;
    }
    checkpoint("mount access r2: exact native mutex exports resolved",0);
    return 0;
}
static int unresolved(ProxMountReport *report)
{
    memset(report,0,sizeof(*report));report->result=-1;report->stage=-1;
    report->active=mount_lease.active;report->retained_fd=mount_lease.active?mount_lease.owned:-1;return -1;
}
static int flush_locked(uint32_t table)
{
    ProxMountView current;
    if(view(&current) || current.table!=table || table!=mount_lease.original.table ||
       current.base!=mount_lease.original.base)return -1;
#ifdef PROX_MOUNT_PLATFORM_TEST
    return prox_mount_platform_test_flush(table,current.base);
#else
    return ((int (*)(void *))(current.base+0xd3cu))((void *)table);
#endif
}
int prox_native_mount_access_probe(ProxMountReport *report)
{
    checkpoint("mount access: extended kernel proof entered",0);
    if(mount_lease.active || resolve())return unresolved(report);
    return prox_mount_access_cycle(&ops,report);
}
int prox_native_mount_begin(ProxMountReport *report)
{
    checkpoint("mount access: extended kernel BEGIN",0);
    if(resolve())return unresolved(report);
    return prox_mount_lease_begin(&ops,&mount_lease,report);
}
int prox_native_mount_end(ProxMountReport *report)
{
    checkpoint("mount access: extended kernel END",0);
    if(resolve())return unresolved(report);
    return prox_mount_lease_end(&ops,flush_locked,&mount_lease,report);
}
int prox_native_mount_active(void){return mount_lease.active;}
