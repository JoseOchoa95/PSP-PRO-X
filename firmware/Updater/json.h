/* Bounded strict JSON reader for the GitHub update contract. GPL-3.0-or-later. */
#ifndef PROX_JSON_H
#define PROX_JSON_H
#include <stddef.h>
enum { PJ_OBJECT, PJ_ARRAY, PJ_STRING, PJ_NUMBER, PJ_TRUE, PJ_FALSE, PJ_NULL };
typedef struct { int type, start, size, next, count; } PjToken;
typedef struct { const char *data; size_t size; PjToken *tokens; int used, capacity; } Pj;
int pj_parse(Pj *j, const char *data, size_t size, PjToken *tokens, int capacity);
int pj_field(const Pj *j, int object, const char *key);
int pj_text(const Pj *j, int token, char *out, size_t capacity);
int pj_equal(const Pj *j, int token, const char *value);
int pj_uint(const Pj *j, int token, unsigned int *value);
#endif
