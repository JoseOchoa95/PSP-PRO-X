#ifndef PROX_NATIVE_QUERY_CACHE_H
#define PROX_NATIVE_QUERY_CACHE_H
/* Boot-time RAM preparation only; no network, flash writes or update verdict. */
void prox_native_query_prepare(void);
int prox_native_query_cached_load(const char **path);
#endif
