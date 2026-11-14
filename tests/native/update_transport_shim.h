/* Isolated native transport tests ONLY. Never linked into PSP builds. */
#ifndef UPDATE_TRANSPORT_SHIM_H
#define UPDATE_TRANSPORT_SHIM_H
#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdint.h>
unsigned int sceKernelGetSystemTimeLow(void);
uint64_t sceKernelGetSystemTimeWide(void);
int sceKernelDelayThread(unsigned int);
int sceNetResolverCreate(int *, void *, int);
int sceNetResolverDelete(int);
int sceNetResolverStartNtoA(int, const char *, struct in_addr *, unsigned int, int);
int sceNetInetSocket(int, int, int);
int sceNetInetSetsockopt(int, int, int, const void *, socklen_t);
int sceNetInetConnect(int, const struct sockaddr *, socklen_t);
int sceNetInetGetErrno(void);
#define SO_NONBLOCK 0x7777
#define sceNetInetSend send
#define sceNetInetRecv recv
#define sceNetInetClose close
#define sceNetInetSelect select
#define sceNetInetGetsockopt getsockopt
#endif
