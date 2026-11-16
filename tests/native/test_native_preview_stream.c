#include <assert.h>
#include <stdio.h>
#include "native_preview_stream.h"
int main(void)
{
    ProxPreviewStream s={0}; unsigned int serial; int fd,n,old;
    prox_preview_stream_reset(&s); assert(prox_preview_stream_open(&s,7)<0);
    assert(prox_preview_stream_offer(&s,1)<0 && !prox_preview_stream_offer(&s,1024*1024));
    fd=prox_preview_stream_open(&s,7); assert(fd>0 && prox_preview_stream_fd(fd));
    assert(prox_preview_stream_open(&s,7)<0 && prox_preview_stream_peek(&s,8,32768,&serial)<0);
    while((n=prox_preview_stream_peek(&s,7,100000,&serial))>0) {
        assert(n<=32768 && prox_preview_stream_read(&s,7,serial+1,n)<0);
        assert(prox_preview_stream_write(&s,7,fd,n)<0);
        assert(prox_preview_stream_read(&s,7,serial,n)==n);
        assert(prox_preview_stream_peek(&s,7,32768,&serial)<0);
        assert(prox_preview_stream_write(&s,8,fd,n)<0 && prox_preview_stream_write(&s,7,fd,n+1)<0);
        assert(prox_preview_stream_write(&s,7,fd,n)==n && s.written<s.total);
    }
    assert(s.written==s.total-32768 && s.read==s.written && !s.pending);
    assert(prox_preview_stream_peek(&s,7,1,&serial)<0);
    old=fd; prox_preview_stream_reset(&s); assert(!prox_preview_stream_offer(&s,1024*1024));
    fd=prox_preview_stream_open(&s,7); assert(fd!=old);
    assert(!prox_preview_stream_close(&s,7,old) && s.opened);
    prox_preview_stream_abort(&s); assert(prox_preview_stream_peek(&s,7,32768,&serial)<0);
    assert(!prox_preview_stream_close(&s,7,fd) && !s.opened);
    assert(prox_preview_stream_close(&s,7,4)<0);
    puts("PASS: synthetic stream ownership/generation/pending-write accounting; full download/EOF impossible; abort/stale-close safe");
    return 0;
}
