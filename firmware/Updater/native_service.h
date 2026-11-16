#ifndef PROX_NATIVE_SERVICE_H
#define PROX_NATIVE_SERVICE_H
extern int prox_native_mode;
void prox_native_worker(int argc,char **argv);
int prox_native_is_cancelled(void);
void prox_native_log_line(const char *line);
void prox_native_resident_fault(void);
int prox_native_trust(void);
#endif
