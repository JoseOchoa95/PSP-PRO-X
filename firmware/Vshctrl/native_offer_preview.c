/* Read-only diagnostic guard for the stock positive-offer UI. No Sony code.
 * No package or native legacy write is allowed. GPL-3.0-or-later. */
#ifdef PROX_NATIVE_PREVIEW_TEST
#include "native_preview_io_shim.h"
#else
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspsdk.h>
#endif
#include <string.h>
#include "native_offer_preview.h"
#include "native_query_provider.h"
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
#include "native_preview_stream.h"
static ProxPreviewStream stream;
static int valid_bytes(const void *p,unsigned int n)
{
#ifdef PROX_NATIVE_PREVIEW_TEST
    return prox_preview_test_bytes(p,n);
#else
    unsigned int address=(unsigned int)p;
    return n && n<=PROX_PREVIEW_CHUNK && address>=0x08800000 && address<=0x0c000000u-n;
#endif
}
static int target_path(const char *path)
{
    char local[96]; const char *component; unsigned int i,n;
    for(i=0;i<sizeof(local);++i) {
        if(!valid_bytes(path+i,1)) return 0;
        local[i]=path[i]; if(!local[i]) break;
    }
    if(i==sizeof(local)) return 0;
    if(strncmp(local,"ms0:/PSP/GAME/",14) && strncmp(local,"ef0:/PSP/GAME/",14) &&
       strncmp(local,"eh0:/PSP/GAME/",14)) return 0;
    component=local+14;
    if(!strcmp(component,"UPDATE/EBOOT.PBP")) return 1;
    if(strncmp(component,"_UPDATE",7)) return 0;
    component+=7; n=0;
    while(component[n]>='0' && component[n]<='9') ++n;
    return (n==0 || n==20) && !strcmp(component+n,"/EBOOT.PBP");
}
#endif
#define DENIED ((int)0x80010013)
int proxNativePreviewDeny(void)
{
#ifdef PROX_NATIVE_OTA
    prox_native_query_event("native blocked legacy filesystem mutation",DENIED);
#else
    prox_native_query_event("PREVIEW blocked native filesystem mutation",DENIED);
#endif
    return DENIED;
}
int proxNativePreviewOpen(const char *path,int flags,int mode)
{
    if(flags!=1) {
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
        if(flags==0x602 && target_path(path)) {
            int owner=sceKernelGetThreadId(),fd; unsigned int mask=pspSdkDisableInterrupts();
            fd=prox_preview_stream_open(&stream,owner); pspSdkEnableInterrupts(mask);
#ifdef PROX_NATIVE_OTA
            if(fd<0 || prox_native_ota_begin(owner,stream.total)) { prox_native_preview_abort(); return DENIED; }
            prox_native_query_event("native virtual progress opened; real package stays private",fd); return fd;
#else
            prox_native_query_event("PREVIEW virtual update opened; no disk file",fd); return fd;
#endif
        }
#endif
        return proxNativePreviewDeny();
    }
    return sceIoOpen(path,flags,mode);
}
int proxNativePreviewWrite(int fd,const void *buffer,int size)
{
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    if(buffer && size>0 && prox_preview_stream_fd(fd)) {
        int owner=sceKernelGetThreadId(),rc; unsigned int mask=pspSdkDisableInterrupts();
        rc=prox_preview_stream_write(&stream,owner,fd,(unsigned int)size); pspSdkEnableInterrupts(mask);
        return rc;
    }
#else
    (void)fd; (void)buffer; (void)size;
#endif
    return proxNativePreviewDeny();
}
int proxNativePreviewClose(int fd)
{
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    if(prox_preview_stream_fd(fd)) {
        int owner=sceKernelGetThreadId(),rc; unsigned int mask=pspSdkDisableInterrupts();
        rc=prox_preview_stream_close(&stream,owner,fd); pspSdkEnableInterrupts(mask); return rc;
    }
    return sceIoClose(fd);
#else
    (void)fd; return proxNativePreviewDeny();
#endif
}
int proxNativePreviewNoop(void)
{
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
#ifdef PROX_NATIVE_OTA
    prox_native_query_event("native virtual metadata; no disk mutation",0); return 0;
#else
    prox_native_query_event("PREVIEW virtual metadata; no disk mutation",0); return 0;
#endif
#else
    return proxNativePreviewDeny();
#endif
}
void prox_native_preview_reset(void)
{
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    unsigned int mask=pspSdkDisableInterrupts(); prox_preview_stream_reset(&stream); pspSdkEnableInterrupts(mask);
#endif
}
void prox_native_preview_offer(unsigned int total)
{
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    unsigned int mask=pspSdkDisableInterrupts(); prox_preview_stream_offer(&stream,total); pspSdkEnableInterrupts(mask);
#else
    (void)total;
#endif
}
void prox_native_preview_abort(void)
{
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    unsigned int mask=pspSdkDisableInterrupts(); prox_preview_stream_abort(&stream); pspSdkEnableInterrupts(mask);
#endif
}
int prox_native_preview_read(void *buffer,unsigned int capacity)
{
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    unsigned int serial,mask; int size,rc,owner=sceKernelGetThreadId();
#ifndef PROX_NATIVE_OTA
    unsigned int i;
#endif
    /* The inspected caller provides 0x19000 bytes. Reject undersized calls
     * rather than stretching a paced diagnostic into unbounded tiny reads. */
    if(capacity<PROX_PREVIEW_CHUNK) return DENIED;
    mask=pspSdkDisableInterrupts(); size=prox_preview_stream_peek(&stream,owner,capacity,&serial); pspSdkEnableInterrupts(mask);
    if(size<=0) {
#ifdef PROX_NATIVE_OTA
        unsigned int received;
        mask=pspSdkDisableInterrupts(); received=stream.read;
        rc=stream.opened && !stream.cancelled && stream.owner==owner && !stream.pending;
        pspSdkEnableInterrupts(mask);
        if(rc) prox_native_ota_wait(owner,received,0); /* Successful LoadExec never returns. */
#endif
#ifdef PROX_NATIVE_OTA
        prox_native_query_event("native transfer stopped or installer handoff returned",DENIED); return DENIED;
#else
        prox_native_query_event("PREVIEW progress completion blocked",DENIED); return DENIED;
#endif
    }
    if(!valid_bytes(buffer,(unsigned int)size)) return DENIED;
    /* A paced virtual stream gives the original UI time to update and cancel.
     * No sleep, kernel I/O or user-buffer copy occurs under the state lock. */
#ifdef PROX_NATIVE_OTA
    {
        unsigned int received;
        mask=pspSdkDisableInterrupts(); received=stream.read; pspSdkEnableInterrupts(mask);
        if(prox_native_ota_wait(owner,received,(unsigned int)size)) return DENIED;
    }
#else
    for(i=0;i<25;++i) {
        mask=pspSdkDisableInterrupts(); rc=stream.cancelled || stream.serial!=serial || !stream.opened; pspSdkEnableInterrupts(mask);
        if(rc) return DENIED;
        sceKernelDelayThread(10000);
    }
#endif
    mask=pspSdkDisableInterrupts(); rc=prox_preview_stream_read(&stream,owner,serial,(unsigned int)size); pspSdkEnableInterrupts(mask);
    if(rc<0) return DENIED;
    memset(buffer,0,(unsigned int)size);
#ifdef PROX_NATIVE_OTA
    prox_native_query_event("native progress: actual private package bytes accounted",size); return size;
#else
    prox_native_query_event("PREVIEW synthetic progress bytes; no package",size); return size;
#endif
#else
    (void)buffer; (void)capacity; return DENIED;
#endif
}
int proxNativePreviewIoctl(int fd,unsigned int command,void *in,int in_size,void *out,int out_size)
{
    /* Inspected stock write-protection/media queries, both output-only. */
    if((command!=0x02125008u && command!=0x02125009u) || in || in_size || !out || out_size!=4)
        return proxNativePreviewDeny();
    return sceIoIoctl(fd,command,in,in_size,out,out_size);
}
int proxNativePreviewDevctl(const char *device,unsigned int command,void *in,int in_size,void *out,int out_size)
{
    /* Inspected stock Memory Stick state query; no input or mutation. */
    if(command!=0x02125802u || in || in_size || !out || out_size!=4) return proxNativePreviewDeny();
    return sceIoDevctl(device,command,in,in_size,out,out_size);
}
void *prox_native_preview_io_function(unsigned int nid)
{
    switch(nid) {
    case 0x109f50bc: return proxNativePreviewOpen;
    case 0x63632449: return proxNativePreviewIoctl;
    case 0x54f5fb11: return proxNativePreviewDevctl;
    case 0x42ec03ac: return proxNativePreviewWrite;
    case 0x779103a0: case 0x06a70004:
    case 0xb8a740f4: case 0xf27a9c51: case 0x1117c65f: return proxNativePreviewNoop;
#if defined(PROX_NATIVE_PROGRESS_PREVIEW) || defined(PROX_NATIVE_OTA)
    case 0x810c4bc3: return proxNativePreviewClose;
#endif
    default: return 0;
    }
}
int prox_native_preview_io_known(unsigned int nid)
{
    if(prox_native_preview_io_function(nid)) return 1;
    switch(nid) {
    case 0x6a638d83: case 0x810c4bc3: case 0xace946e8:
    case 0xb29ddf9c: case 0xeb092469: case 0x27eb27b8: return 1;
    default: return 0;
    }
}
