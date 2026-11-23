/* Portable synthetic stream accounting; never a real update executable. */
#ifndef PROX_NATIVE_PREVIEW_STREAM_H
#define PROX_NATIVE_PREVIEW_STREAM_H
#define PROX_PREVIEW_CHUNK 32768u
#define PROX_PREVIEW_FD_TAG 0x50580000u
typedef struct {
    unsigned int serial,total,read,written,pending,fd;
    int offered,opened,cancelled,owner;
} ProxPreviewStream;
void prox_preview_stream_reset(ProxPreviewStream *);
int prox_preview_stream_offer(ProxPreviewStream *,unsigned int total);
int prox_preview_stream_open(ProxPreviewStream *,int owner);
int prox_preview_stream_peek(const ProxPreviewStream *,int owner,unsigned int capacity,unsigned int *serial);
int prox_preview_stream_read(ProxPreviewStream *,int owner,unsigned int serial,unsigned int size);
int prox_preview_stream_write(ProxPreviewStream *,int owner,int fd,unsigned int size);
int prox_preview_stream_close(ProxPreviewStream *,int owner,int fd);
void prox_preview_stream_abort(ProxPreviewStream *);
int prox_preview_stream_finish(ProxPreviewStream *,int owner,unsigned int capacity,unsigned int serial,int ready);
int prox_preview_stream_fd(int fd);
#endif
