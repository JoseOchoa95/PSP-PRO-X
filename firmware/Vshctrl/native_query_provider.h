#ifndef PROX_NATIVE_QUERY_PROVIDER_H
#define PROX_NATIVE_QUERY_PROVIDER_H
void *prox_native_query_function(unsigned int nid);
void prox_native_query_event(const char *,int);
int prox_native_query_start(unsigned int lease);
int prox_native_query_is_loading(void);
void prox_native_query_load_failed(void);
void prox_native_query_load_failed_reason(unsigned int);
const char *proxNativeQueryTextKey(const char *);
int prox_native_ota_begin(int owner,unsigned int total);
int prox_native_ota_wait(int owner,unsigned int received,unsigned int wanted);
#if defined(PROX_NATIVE_INSTALL_PREVIEW) || defined(PROX_NATIVE_BACKGROUND)
int prox_native_install_preview_locked(void);
unsigned int prox_native_completion_generation(void);
#endif
#ifdef PROX_NATIVE_BACKGROUND
int prox_native_install_critical(void);
int prox_native_install_completion_owner(int,unsigned int);
#endif
#endif
