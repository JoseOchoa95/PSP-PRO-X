#ifndef PROX_UPDATE_POLICY_H
#define PROX_UPDATE_POLICY_H
#include <stddef.h>
int prox_url(const char *url, char *host, size_t capacity, const char **path);
int prox_version(const char *text, unsigned int version[3]);
int prox_compare(const unsigned int a[3], const unsigned int b[3]);
int prox_stable(int draft, int prerelease, const char *tag);
#endif
