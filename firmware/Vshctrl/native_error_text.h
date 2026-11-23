/* Existing native resource keys only; no renderer or translated text. */
#ifndef PROX_NATIVE_ERROR_TEXT_H
#define PROX_NATIVE_ERROR_TEXT_H
#include <stddef.h>
#include <stdint.h>
const char *prox_native_error_select(unsigned int,const char *,const char *,const char *);
int prox_native_error_code(unsigned int *,size_t,uintptr_t,unsigned int,uintptr_t);
unsigned int prox_native_error_prepare(void);
const char *prox_native_error_redirect(unsigned int,const char *);
int prox_native_notice_code(unsigned int *,size_t,uintptr_t,unsigned int,unsigned int,uintptr_t);
unsigned int prox_native_completion_noop_address(void);
int prox_native_back_code(unsigned int *,size_t,uintptr_t,unsigned int,uintptr_t);
unsigned int prox_native_completion_back_address(void);
#endif
