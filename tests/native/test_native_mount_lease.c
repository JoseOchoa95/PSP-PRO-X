#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_mount_access.h"
static ProxMountView volume;static int held,fail,phase,opens,closes,views;
static int64_t old_cursor,new_cursor;
static int view(ProxMountView *v)
{++views;if((fail==1 && views==1)||(fail==4 && views==2)||(fail==9 && views==3)||(fail==11 && phase==2))return -1;*v=volume;return 0;}
static int open_owned(void){assert(!held);++opens;return fail==2?-9:29;}
static int lock(int id){assert(id==31 && !held);if((fail==3 && phase==1)||(fail==12 && phase==2))return -9;held=1;return 0;}
static int unlock(int id){assert(id==31 && held);if(fail==19 || fail==20)return -9;held=0;return 0;}
static int64_t seek(int fd,int64_t pos,int whence)
{
    assert(held && (fd==17 || fd==29));
    if((fail==5 && phase==1 && fd==17)||(fail==6 && phase==1 && fd==29)||
       (fail==14 && phase==2 && fd==29)||(fail==15 && phase==2 && fd==17))return -9;
    if(whence==1){assert(!pos);return fd==17?old_cursor:new_cursor;}
    assert(whence==0);if(fd==17)old_cursor=pos;else new_cursor=pos;return pos;
}
static int set_fd(uint32_t table,int fd)
{assert(held && table==volume.table);if((fail==7 && phase==1 && fd==29)||(fail==17 && phase==2 && fd==17))return -9;volume.fd=fd;return 0;}
static int set_flags(uint32_t table,uint32_t flags)
{assert(held && table==volume.table);if((fail==8 && phase==1 && flags==9)||(fail==16 && phase==2 && (flags&2)))return -9;volume.flags=flags;return 0;}
static int close_owned(int fd)
{assert(!held && fd==29 && volume.fd!=29);if(fail==18)return -9;++closes;return 0;}
static int flush(uint32_t table)
{assert(held && phase==2 && table==volume.table && volume.fd==29);if(fail==13)return -9;volume.flags&=~4u;return 0;}
int main(void)
{
    const ProxMountOps ops={view,open_owned,lock,unlock,seek,set_fd,set_flags,close_owned};ProxMountLease s;ProxMountReport r;int n,rc;
    for(n=0;n<=20;++n){
        memset(&volume,0,sizeof(volume));volume.base=0x880b7500;volume.table=0x880ce3b8;volume.mutex_address=0x880c2cc4;volume.mutex=31;volume.fd=17;volume.flags=11;volume.references=5;
        memset(&s,0,sizeof(s));held=opens=closes=views=0;old_cursor=0x123400;new_cursor=0;fail=n;phase=1;
        rc=prox_mount_lease_begin(&ops,&s,&r);
        if(n>=1 && n<=9){assert(rc<0 && !s.active && !held && volume.fd==17 && volume.flags==11);continue;}
        if(n==19 || n==20){assert(rc<0 && s.active && s.fault && held && !closes);phase=2;assert(prox_mount_lease_end(&ops,flush,&s,&r)<0 && held);continue;}
        assert(!rc && s.active && s.mapped && !held && volume.fd==29 && volume.flags==9 && !closes);
        assert(prox_mount_lease_begin(&ops,&s,&r)<0 && s.active && opens==1);
        phase=2;new_cursor+=0x1000;volume.flags|=4u;volume.references=8;
        rc=prox_mount_lease_end(&ops,flush,&s,&r);
        if(n>=11 && n<=18){assert(rc<0 && s.active && r.active && r.retained_fd==29 && !closes && !held);fail=0;assert(!prox_mount_lease_end(&ops,flush,&s,&r));}
        else assert(!rc);
        assert(!s.active && !s.mapped && !held && r.restored && !r.active && r.retained_fd==-1 && volume.fd==17 && volume.flags==11 && volume.references==8 && closes==1 && old_cursor==new_cursor);
        assert(prox_mount_lease_end(&ops,flush,&s,&r)<0 && closes==1);
    }
    puts("PASS: actual mounted access lease, native flush under mutex before cursor/RO restore, evolving XMB references preserved, every boundary failure/retry, no close while referenced, no double-BEGIN or false committed cleanup");
}
