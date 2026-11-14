/* Bounded PSP resolver supervisor. GPL-3.0-or-later. */
#ifndef PROX_RESOLVER_H
#define PROX_RESOLVER_H
struct in_addr;
int prox_psp_resolve(const char *host,struct in_addr *address,unsigned int request_start);
#endif
