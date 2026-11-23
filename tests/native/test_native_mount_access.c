#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_mount_access.h"
static ProxMountView volume;
static int held,opens,closes,views,fd_sets,flag_sets,fail,unlock_count;
static int view(ProxMountView *v)
{ ++views;if((fail==1 && views==1)||(fail==4 && views==2)||(fail==8 && views==3)||(fail==12 && views>=4))return -1;*v=volume;if(fail==13 && views==2)v->table+=628;return 0; }
static int open_owned(void){assert(!held);++opens;return fail==2?-9:fail==14?17:29;}
static int lock(int id){assert(id==31 && !held);if(fail==3)return -9;held=1;return 0;}
static int unlock(int id){assert(id==31 && held);held=0;++unlock_count;return fail==10?-9:0;}
static int64_t seek(int fd,int64_t pos,int whence)
{ assert(held);if(fd==17){assert(pos==0 && whence==1);return fail==5?-9:0x123456780LL;}assert(fd==29 && whence==0 && pos==0x123456780LL);return fail==6?-9:pos; }
static int set_fd(uint32_t table,int fd)
{assert(held && table==volume.table);++fd_sets;if((fail==7 && fd_sets==1)||(fail==15 && fd_sets>1))return -9;assert(fd==17 || fd==29);volume.fd=fd;return 0;}
static int set_flags(uint32_t table,uint32_t flags)
{assert(held && table==volume.table);++flag_sets;if(fail==9 && flag_sets==1)return -9;assert(flags==9 || flags==11);volume.flags=flags;return 0;}
static int close_owned(int fd){assert(fd==29 && !held && volume.fd!=fd);++closes;return fail==11?-9:0;}
int main(void)
{
    const ProxMountOps ops={view,open_owned,lock,unlock,seek,set_fd,set_flags,close_owned};ProxMountReport r;int n;
    for(n=0;n<=15;++n){
        memset(&volume,0,sizeof(volume));volume.base=0x880b7500;volume.table=0x880ce3b8;volume.mutex_address=0x880c2cc4;volume.mutex=31;volume.fd=17;volume.flags=11;volume.references=5;
        held=opens=closes=views=fd_sets=flag_sets=unlock_count=0;fail=n;
        if(!n){assert(!prox_mount_access_cycle(&ops,&r));assert(r.restored && r.stage==9 && r.flags_during==9 && r.refs_after==5 && r.flags_after==11 && r.original_cursor==0x123456780LL && r.owned_cursor==r.original_cursor);}
        else assert(prox_mount_access_cycle(&ops,&r)<0);
        assert(!held && volume.references==5);
        if(n==15){assert(volume.fd==29 && volume.flags==11 && r.retained_fd==29 && !closes && !r.restored);}
        else {assert(volume.fd==17 && volume.flags==11);if(n==12)assert(r.retained_fd==29 && !closes);}
        if(n>=4 && n!=14)assert(unlock_count==1);
        if(n==1)assert(!opens && !closes);
        if(n==2 || n==14)assert(!closes && !unlock_count);
    }
    assert(prox_mount_access_cycle(NULL,&r)<0 && prox_mount_access_cycle(&ops,NULL)<0);
    puts("PASS: actual access cycle, 64-bit cursor, mutex ownership, every failure stage, exact restore and never closing original/referenced descriptor; no firmware write operation");
}
