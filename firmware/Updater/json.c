/* Strict bounded JSON, no allocation and no ambiguous duplicate object keys.
 * Keys are restricted to unescaped ASCII; value strings accept JSON escapes.
 * Unused GitHub fields are parsed but never interpreted. GPL-3.0-or-later. */
#include "json.h"
#include <string.h>
#include <limits.h>

static void space(const Pj *j, size_t *p)
{ while (*p < j->size && (j->data[*p] == ' ' || j->data[*p] == '\r' || j->data[*p] == '\n' || j->data[*p] == '\t')) ++*p; }
static int hex(char c)
{ return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1; }
static int string(Pj *j, size_t *p, int key)
{
    size_t start = ++*p;
    while (*p < j->size) {
        unsigned char c = (unsigned char)j->data[(*p)++];
        if (c == '"') return (int)start;
        if (c < 32 || (key && (c == '\\' || c > 126))) return -1;
        if (c >= 128) {
            unsigned int code,minimum; int count,k;
            if (c >= 0xc2 && c <= 0xdf) { code=c & 31; minimum=128; count=1; }
            else if (c >= 0xe0 && c <= 0xef) { code=c & 15; minimum=2048; count=2; }
            else if (c >= 0xf0 && c <= 0xf4) { code=c & 7; minimum=65536; count=3; }
            else return -1;
            for (k=0;k<count;++k) {
                unsigned char next;
                if (*p == j->size) return -1;
                next=(unsigned char)j->data[(*p)++];
                if ((next & 0xc0) != 0x80) return -1;
                code=(code<<6) | (next & 63);
            }
            if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return -1;
            continue;
        }
        if (c == '\\') {
            int i;
            if (*p == j->size) return -1;
            c = (unsigned char)j->data[(*p)++];
            if (c == 'u') {
                unsigned int code = 0;
                for (i = 0; i < 4; ++i) {
                    int n;
                    if (*p == j->size || (n = hex(j->data[(*p)++])) < 0) return -1;
                    code = code*16+(unsigned int)n;
                }
                if (code >= 0xd800 && code <= 0xdbff) {
                    unsigned int low = 0;
                    if (*p+6 > j->size || j->data[*p] != '\\' || j->data[*p+1] != 'u') return -1;
                    *p += 2;
                    for (i = 0; i < 4; ++i) { int n = hex(j->data[(*p)++]); if (n < 0) return -1; low=low*16+(unsigned int)n; }
                    if (low < 0xdc00 || low > 0xdfff) return -1;
                } else if (code >= 0xdc00 && code <= 0xdfff) return -1;
            } else if (!c || !strchr("\"\\/bfnrt", c)) return -1;
        }
    }
    return -1;
}
static int value(Pj *j, size_t *p, int depth)
{
    int id, type;
    size_t start;
    char c;
    if (depth > 16 || j->used == j->capacity) return -1;
    space(j,p); if (*p == j->size) return -1;
    id = j->used++; start = *p; c = j->data[*p];
    j->tokens[id].count=0;
    if (c == '{' || c == '[') {
        char end = c == '{' ? '}' : ']';
        type = c == '{' ? PJ_OBJECT : PJ_ARRAY;
        ++*p; space(j,p);
        if (*p < j->size && j->data[*p] != end) while (1) {
            if (type == PJ_OBJECT) {
                int key, s, other;
                if (j->used == j->capacity || j->data[*p] != '"') return -1;
                key = j->used++;
                s = string(j,p,1); if (s < 0) return -1;
                j->tokens[key].type=PJ_STRING; j->tokens[key].start=s;
                j->tokens[key].size=(int)*p-s-1; j->tokens[key].next=key+1; j->tokens[key].count=0;
                for (other=id+1; other < key; other=j->tokens[other+1].next)
                    if (j->tokens[other].size == j->tokens[key].size &&
                        !memcmp(j->data+j->tokens[other].start,j->data+s,(size_t)j->tokens[key].size)) return -1;
                space(j,p); if (*p == j->size || j->data[(*p)++] != ':') return -1;
            }
            if (value(j,p,depth+1) < 0) return -1;
            ++j->tokens[id].count; space(j,p);
            if (*p == j->size) return -1;
            if (j->data[*p] == end) break;
            if (j->data[(*p)++] != ',') return -1;
            space(j,p); if (*p == j->size || j->data[*p] == end) return -1;
        }
        if (*p == j->size || j->data[(*p)++] != end) return -1;
    } else if (c == '"') {
        int s=string(j,p,0); if (s < 0) return -1;
        start=(size_t)s; type=PJ_STRING;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        type=PJ_NUMBER;
        if (c == '-') { ++*p; if (*p == j->size) return -1; }
        if (j->data[*p] == '0') ++*p;
        else {
            if (j->data[*p] < '1' || j->data[*p] > '9') return -1;
            while (*p < j->size && j->data[*p] >= '0' && j->data[*p] <= '9') ++*p;
        }
        if (*p < j->size && j->data[*p] == '.') {
            size_t n=++*p;
            while (*p < j->size && j->data[*p] >= '0' && j->data[*p] <= '9') ++*p;
            if (*p == n) return -1;
        }
        if (*p < j->size && (j->data[*p] == 'e' || j->data[*p] == 'E')) {
            size_t n;
            ++*p; if (*p < j->size && (j->data[*p] == '+' || j->data[*p] == '-')) ++*p;
            n=*p; while (*p < j->size && j->data[*p] >= '0' && j->data[*p] <= '9') ++*p;
            if (*p == n) return -1;
        }
    } else {
        const char *literal;
        if (c == 't') { literal="true"; type=PJ_TRUE; }
        else if (c == 'f') { literal="false"; type=PJ_FALSE; }
        else if (c == 'n') { literal="null"; type=PJ_NULL; }
        else return -1;
        if (j->size-*p < strlen(literal) || memcmp(j->data+*p,literal,strlen(literal))) return -1;
        *p += strlen(literal);
    }
    j->tokens[id].type=type; j->tokens[id].start=(int)start;
    j->tokens[id].size=(int)(*p-start)-(type == PJ_STRING ? 1 : 0);
    j->tokens[id].next=j->used;
    return id;
}
int pj_parse(Pj *j, const char *data, size_t size, PjToken *tokens, int capacity)
{
    size_t p=0;
    if (!j || !data || !tokens || size > 256*1024 || capacity < 1) return -1;
    j->data=data; j->size=size; j->tokens=tokens; j->used=0; j->capacity=capacity;
    if (value(j,&p,0) < 0) return -1;
    space(j,&p); return p == size ? 0 : -1;
}
int pj_field(const Pj *j, int object, const char *key)
{
    int i;
    if (object < 0 || object >= j->used || j->tokens[object].type != PJ_OBJECT) return -1;
    for (i=object+1; i < j->tokens[object].next; i=j->tokens[i+1].next)
        if ((size_t)j->tokens[i].size == strlen(key) && !memcmp(j->data+j->tokens[i].start,key,strlen(key))) return i+1;
    return -1;
}
int pj_text(const Pj *j, int token, char *out, size_t capacity)
{
    const PjToken *t;
    size_t n=0;
    int i;
    if (token < 0 || token >= j->used || j->tokens[token].type != PJ_STRING || !capacity) return -1;
    t=&j->tokens[token];
    for (i=0; i < t->size; ++i) {
        unsigned char c=(unsigned char)j->data[t->start+i];
        if (c == '\\') {
            c=(unsigned char)j->data[t->start+(++i)];
            if (c == 'u') {
                unsigned int code=0; int k;
                for (k=0;k<4;++k) code=code*16+(unsigned int)hex(j->data[t->start+(++i)]);
                /* Product fields are deliberately printable ASCII only. */
                if (code < 32 || code > 126) return -1;
                c=(unsigned char)code;
            } else if (c != '"' && c != '\\' && c != '/') return -1;
        }
        if (c < 32 || c > 126 || n+1 >= capacity) return -1;
        out[n++]=(char)c;
    }
    out[n]=0; return 0;
}
int pj_equal(const Pj *j, int token, const char *expected)
{ char text[256]; return !pj_text(j,token,text,sizeof(text)) && !strcmp(text,expected); }
int pj_uint(const Pj *j, int token, unsigned int *out)
{
    const PjToken *t;
    unsigned int n=0; int i;
    if (token < 0 || token >= j->used || j->tokens[token].type != PJ_NUMBER) return -1;
    t=&j->tokens[token];
    for (i=0;i<t->size;++i) {
        char c=j->data[t->start+i];
        if (c < '0' || c > '9' || n > (UINT_MAX-(unsigned int)(c-'0'))/10) return -1;
        n=n*10+(unsigned int)(c-'0');
    }
    *out=n; return 0;
}
