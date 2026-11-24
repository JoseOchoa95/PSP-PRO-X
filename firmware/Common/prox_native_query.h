/* Private bounded service protocol; independent of PRO/Infinity ABI. GPL-3.0-or-later. */
#ifndef PROX_NATIVE_QUERY_H
#define PROX_NATIVE_QUERY_H
#define PROX_NQ_PROTOCOL 3u
enum { PROX_NQ_REGISTER=1,PROX_NQ_POLL,PROX_NQ_RESULT,PROX_NQ_STATUS,PROX_NQ_FAULT,
       PROX_NQ_PROGRESS,PROX_NQ_STAGED,PROX_NQ_NOTICE_ARM,PROX_NQ_NOTICE_STATUS };
enum { PROX_NQ_IDLE=0,PROX_NQ_CHECK=1,PROX_NQ_DOWNLOAD,PROX_NQ_INSTALL };
#ifdef PROX_NATIVE_DEDICATED_OTA
#define PROX_NQ_BOOT_CLEANUP 4u
#endif
enum { PROX_NQ_QUERY=0,PROX_NQ_TRANSFER,PROX_NQ_VERIFIED,PROX_NQ_HANDOFF,PROX_NQ_FAILED };
enum { PROX_NQ_CAUSE_NONE=0,PROX_NQ_CAUSE_STORAGE,PROX_NQ_CAUSE_POWER,PROX_NQ_CAUSE_CLIENT };
typedef struct {
    unsigned int protocol,lease,generation,command,cancel;
    int result;
    unsigned int package_size,pages,candidates,downloaded,phase,failure;
    char version[40];
    char summary[241];
} ProxNativeQueryMessage;
int proxNativeQueryService(int action,ProxNativeQueryMessage *message,unsigned int size);
#endif
