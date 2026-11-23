/* Virtual-only progress stream. Caller serializes state access. Normal reads
 * withhold completion; explicit diagnostic finish requires a guarded caller.
 * GPL-3.0-or-later. */
#include "native_preview_stream.h"
#include <string.h>
void prox_preview_stream_reset(ProxPreviewStream *s)
{
    unsigned int serial=s->serial+1;
    if(!serial) serial=1;
    memset(s,0,sizeof(*s)); s->serial=serial;
}
int prox_preview_stream_offer(ProxPreviewStream *s,unsigned int total)
{
    if(!s->serial || s->opened || s->cancelled || total<2*PROX_PREVIEW_CHUNK || total>16u*1024u*1024u) return -1;
    s->total=total; s->offered=1; return 0;
}
int prox_preview_stream_fd(int fd) { return ((unsigned int)fd&0xffff0000u)==PROX_PREVIEW_FD_TAG; }
int prox_preview_stream_open(ProxPreviewStream *s,int owner)
{
    if(!s->offered || s->opened || s->cancelled || owner<=0) return -1;
    s->owner=owner; s->opened=1; s->fd=PROX_PREVIEW_FD_TAG|(s->serial&0xffffu); return (int)s->fd;
}
int prox_preview_stream_peek(const ProxPreviewStream *s,int owner,unsigned int capacity,unsigned int *serial)
{
    unsigned int size;
    if(!s->opened || s->cancelled || s->owner!=owner || !capacity || s->pending || s->read>=s->total) return -1;
    size=capacity<PROX_PREVIEW_CHUNK?capacity:PROX_PREVIEW_CHUNK;
    /* Always leave at least one complete chunk unread, regardless of the
     * caller's capacity. No EOF or full byte count can authorize an installer. */
    if(s->total-s->read<=PROX_PREVIEW_CHUNK) return -1;
    if(size>s->total-s->read-PROX_PREVIEW_CHUNK) size=s->total-s->read-PROX_PREVIEW_CHUNK;
    *serial=s->serial; return (int)size;
}
int prox_preview_stream_read(ProxPreviewStream *s,int owner,unsigned int serial,unsigned int size)
{
    unsigned int current; int expected=prox_preview_stream_peek(s,owner,size,&current);
    if(serial!=s->serial || expected<0 || (unsigned int)expected!=size) return -1;
    s->read+=size; s->pending=size; return (int)size;
}
int prox_preview_stream_write(ProxPreviewStream *s,int owner,int fd,unsigned int size)
{
    if(!s->opened || s->cancelled || s->owner!=owner || (unsigned int)fd!=s->fd || !size || size!=s->pending ||
       s->written>s->read || size>s->read-s->written) return -1;
    s->written+=size; s->pending=0; return (int)size;
}
int prox_preview_stream_close(ProxPreviewStream *s,int owner,int fd)
{
    if(!prox_preview_stream_fd(fd)) return -1;
    if(s->opened && s->owner==owner && s->fd==(unsigned int)fd) { s->opened=0; s->cancelled=1; }
    return 0; /* Never pass a stale synthetic descriptor to the real kernel. */
}
void prox_preview_stream_abort(ProxPreviewStream *s) { s->cancelled=1; }
int prox_preview_stream_finish(ProxPreviewStream *s,int owner,unsigned int capacity,unsigned int serial,int ready)
{
    if(ready!=1 || !s->opened || s->cancelled || s->owner!=owner || owner<=0 || s->serial!=serial ||
       s->pending || s->written!=s->read || s->total<PROX_PREVIEW_CHUNK ||
       s->read!=s->total-PROX_PREVIEW_CHUNK || capacity<PROX_PREVIEW_CHUNK) return -1;
    s->read+=PROX_PREVIEW_CHUNK;s->pending=PROX_PREVIEW_CHUNK;return PROX_PREVIEW_CHUNK;
}
