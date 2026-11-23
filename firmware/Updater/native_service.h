#ifndef PROX_NATIVE_SERVICE_H
#define PROX_NATIVE_SERVICE_H
extern int prox_native_mode;
void prox_native_worker(int argc,char **argv);
int prox_native_is_cancelled(void);
void prox_native_log_line(const char *line);
void prox_native_resident_fault(void);
int prox_native_trust(void);
int prox_native_completion_prepare(const unsigned short *text,unsigned int count);
int prox_native_completion_pending(void);
#include "prox_native_install.h"
int prox_native_install_exchange(int,ProxNativeInstallMessage *);
int prox_native_handoff_close(void);
#endif
