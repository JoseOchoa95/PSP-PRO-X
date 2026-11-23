/* One job's mounted FAT access lease. Shell handles/cache stay live. The
 * original driver flush runs while its own mutex is held, before cursor/RO
 * restoration. Never clear references or close a referenced descriptor.
 * GPL-3.0-or-later. */
#include <string.h>
#include "native_mount_access.h"
static void report_init(ProxMountReport *r,ProxMountLease *s)
{
    memset(r,0,sizeof(*r));r->result=-1;r->original_fd=r->owned_fd=r->retained_fd=-1;
    r->original_cursor=r->owned_cursor=-1;r->active=s->active;
    if(s->active){r->original_fd=s->original.fd;r->owned_fd=r->retained_fd=s->owned;r->flags_before=s->original.flags;}
}
static int identity(const ProxMountView *a,const ProxMountView *b)
{ return a->base==b->base && a->table==b->table && a->mutex_address==b->mutex_address && a->mutex==b->mutex && a->references<=65535u; }
static int finish(const ProxMountOps *o,ProxMountLease *s,ProxMountReport *r,int locked,int success)
{
    if(locked){r->unlock_result=o->unlock(s->original.mutex);if(r->unlock_result<0){s->fault=1;success=0;}}
    if(!s->mapped && !s->fault){
        r->close_result=o->close_owned(s->owned);
        if(r->close_result>=0){s->active=0;r->retained_fd=-1;}else success=0;
    }
    r->active=s->active;if(s->active)r->retained_fd=s->owned;
    r->result=success?0:-1;return r->result;
}
int prox_mount_lease_begin(const ProxMountOps *o,ProxMountLease *s,ProxMountReport *r)
{
    ProxMountView initial,before,after;int locked=0;
    if(!o || !s || !r)return -1;
    report_init(r,s);r->stage=1;if(s->active || s->fault)return -1;
    if(o->view(&initial) || (initial.flags&7u)!=3u || initial.fd<0 || initial.mutex<0 || initial.references>65535u)return -1;
    r->stage=2;r->open_result=o->open_owned();if(r->open_result<0)return -1;
    if(r->open_result==initial.fd){r->retained_fd=r->open_result;return -1;}
    s->original=initial;s->owned=r->open_result;s->active=1;s->mapped=0;
    r->original_fd=initial.fd;r->owned_fd=s->owned;r->retained_fd=s->owned;
    r->stage=3;r->lock_result=o->lock(initial.mutex);if(r->lock_result<0)goto failure;
    locked=1;
    r->stage=4;if(o->view(&before) || !identity(&before,&initial) || before.fd!=initial.fd || (before.flags&7u)!=3u)goto failure;
    s->original=before;r->flags_before=before.flags;r->refs_before=before.references;
    r->stage=5;r->original_cursor=o->seek(before.fd,0,1);if(r->original_cursor<0)goto failure;
    r->stage=6;r->owned_cursor=o->seek(s->owned,r->original_cursor,0);if(r->owned_cursor!=r->original_cursor)goto failure;
    r->stage=7;s->mapped=1;
    if(o->set_fd(before.table,s->owned) || o->set_flags(before.table,before.flags&~2u))goto failure;
    r->stage=8;if(o->view(&after) || !identity(&after,&before) || after.fd!=s->owned ||
       after.flags!=(before.flags&~2u) || after.references!=before.references)goto failure;
    r->flags_during=after.flags;r->active=1;r->stage=9;
    return finish(o,s,r,locked,1);
failure:
    /* No firmware I/O has run in BEGIN. Restore under the lock when possible;
     * otherwise retain the owned handle and let END perform guarded cleanup. */
    if(locked && s->mapped && !o->set_flags(s->original.table,s->original.flags) &&
       !o->set_fd(s->original.table,s->original.fd) && !o->view(&after) &&
       identity(&after,&s->original) && after.fd==s->original.fd && after.flags==s->original.flags) {
        s->mapped=0;r->restored=1;r->flags_after=after.flags;r->refs_after=after.references;
    }
    return finish(o,s,r,locked,0);
}
int prox_mount_lease_end(const ProxMountOps *o,int (*flush)(uint32_t),ProxMountLease *s,ProxMountReport *r)
{
    ProxMountView before,after;uint32_t readonly_flags;
    if(!o || !flush || !s || !r)return -1;
    report_init(r,s);r->stage=10;if(!s->active || s->fault)return -1;
    if(!s->mapped){r->restored=1;return finish(o,s,r,0,1);}
    r->stage=11;r->lock_result=o->lock(s->original.mutex);if(r->lock_result<0)return -1;
    r->stage=12;if(o->view(&before) || !identity(&before,&s->original) || !(before.flags&1u))goto failure;
    r->flags_before=before.flags;r->refs_before=before.references;
    if(before.fd==s->original.fd && (before.flags&2u) && !(before.flags&4u)) {
        s->mapped=0;r->restored=1;r->flags_after=before.flags;r->refs_after=before.references;
        return finish(o,s,r,1,1);
    }
    if(before.fd!=s->owned)goto failure;
    /* Native per-volume flush propagates its error; the public global sync
     * discards per-volume errors. Hold the original mutex across this flush,
     * final cursor transfer and descriptor/RO restoration (no TOCTOU gap). */
    r->stage=13;r->flush_result=flush(before.table);if(r->flush_result<0)goto failure;
    if(o->view(&after) || !identity(&after,&before) || after.fd!=s->owned || (after.flags&4u) ||
       after.references!=before.references)goto failure;
    readonly_flags=after.flags|2u;
    r->stage=14;r->owned_cursor=o->seek(s->owned,0,1);if(r->owned_cursor<0)goto failure;
    r->stage=15;r->original_cursor=o->seek(s->original.fd,r->owned_cursor,0);if(r->original_cursor!=r->owned_cursor)goto failure;
    r->stage=16;
    /* RO first; a failed flag store must never expose the old RO descriptor
     * behind a writable mounted filesystem. */
    if(o->set_flags(before.table,readonly_flags) || o->set_fd(before.table,s->original.fd))goto failure;
    r->stage=17;if(o->view(&after) || !identity(&after,&before) || after.fd!=s->original.fd ||
       after.flags!=readonly_flags || after.references!=before.references)goto failure;
    s->mapped=0;r->restored=1;r->flags_after=after.flags;r->refs_after=after.references;r->stage=19;
    return finish(o,s,r,1,1);
failure:
    return finish(o,s,r,1,0);
}
