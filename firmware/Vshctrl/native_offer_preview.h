#ifndef PROX_NATIVE_OFFER_PREVIEW_H
#define PROX_NATIVE_OFFER_PREVIEW_H
int proxNativePreviewOpen(const char *,int,int);
int proxNativePreviewWrite(int,const void *,int);
int proxNativePreviewClose(int);
int prox_native_preview_io_known(unsigned int nid);
void *prox_native_preview_io_function(unsigned int nid);
void prox_native_preview_reset(void);
void prox_native_preview_offer(unsigned int total);
void prox_native_preview_abort(void);
int prox_native_preview_read(void *buffer,unsigned int capacity);
#endif
