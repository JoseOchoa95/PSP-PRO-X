/* A single no-file-write transition, with the original FAT mutex held for
 * every changed field. Never unmount, close shell handles, alter reference
 * counts, write firmware files or leave a requested read/write lease.
 * GPL-3.0-or-later. */
#include <string.h>
#include "native_mount_access.h"
static int valid_view(const ProxMountView *v)
{ return (v->flags&3u)==3u && v->fd>=0 && v->mutex>=0 && v->references<=65535u; }
static int same_mount(const ProxMountView *a,const ProxMountView *b)
{ return a->base==b->base && a->table==b->table && a->mutex_address==b->mutex_address && a->mutex==b->mutex && a->fd==b->fd; }
int prox_mount_access_cycle(const ProxMountOps *o,ProxMountReport *r)
{
    ProxMountView initial,before,after;int owned=-1,locked=0,changed=0,i,safe_close=1;
    if(!r)return -1;
    memset(r,0,sizeof(*r));r->result=-1;r->original_fd=r->owned_fd=r->retained_fd=-1;
    r->original_cursor=r->owned_cursor=-1;
    if(!o || !o->view || !o->open_owned || !o->lock || !o->unlock || !o->seek ||
       !o->set_fd || !o->set_flags || !o->close_owned)return -1;
    r->stage=1;if(o->view(&initial) || !valid_view(&initial))return -1;
    r->original_fd=initial.fd;
    r->stage=2;owned=o->open_owned();r->open_result=owned;r->owned_fd=owned;
    if(owned<0)return -1;
    /* A distinct owned descriptor is essential: never close the old one. */
    if(owned==initial.fd){r->retained_fd=owned;return -1;}
    r->stage=3;r->lock_result=o->lock(initial.mutex);if(r->lock_result<0)goto done;
    locked=1;
    r->stage=4;if(o->view(&before) || !valid_view(&before) || !same_mount(&initial,&before))goto done;
    r->flags_before=before.flags;r->refs_before=before.references;
    r->stage=5;r->original_cursor=o->seek(before.fd,0,1);if(r->original_cursor<0)goto done;
    r->stage=6;r->owned_cursor=o->seek(owned,r->original_cursor,0);
    if(r->owned_cursor!=r->original_cursor)goto done;
    /* There is deliberately no raw/file read, write, sync or callback between
     * substitution and restoration. Existing FAT caches/handles stay in place. */
    r->stage=7;changed=1;safe_close=0;
    if(o->set_fd(before.table,owned) || o->set_flags(before.table,before.flags&~2u))goto restore;
    r->stage=8;
    if(o->view(&after) || after.base!=before.base || after.table!=before.table ||
       after.mutex!=before.mutex || after.mutex_address!=before.mutex_address || after.fd!=owned ||
       after.flags!=(before.flags&~2u) || after.references!=before.references)goto restore;
    r->flags_during=after.flags;r->result=0;
restore:
    /* Restore RO before the original backing descriptor. Retry readback once;
     * never close a descriptor that could still belong to the mounted volume. */
    for(i=0;i<2;++i) {
        int a=o->set_flags(before.table,before.flags),b=o->set_fd(before.table,before.fd);
        if(!a && !b && !o->view(&after) && same_mount(&before,&after) &&
           after.flags==before.flags && after.references==before.references) {
            r->restored=1;r->flags_after=after.flags;r->refs_after=after.references;safe_close=1;break;
        }
        r->result=-1;
    }
    if(!r->restored)r->result=-1;
done:
    if(locked){r->unlock_result=o->unlock(initial.mutex);if(r->unlock_result<0)r->result=-1;}
    if(safe_close){r->close_result=o->close_owned(owned);if(r->close_result<0){r->retained_fd=owned;r->result=-1;}}
    else r->retained_fd=owned;
    if(!changed)r->result=-1;
    if(!r->result)r->stage=9;
    return r->result;
}
