/* PRO-X diagnostic protocol helpers, GPL-3.0-or-later. */
#include "protocol.h"
#include <string.h>
#include <limits.h>

static unsigned int u16(const unsigned char *p) { return (p[0] << 8) | p[1]; }

int nd_dns_query(unsigned char *out, size_t cap, unsigned short id, const char *host)
{
    size_t n = 12, start = 0, i, len = strlen(host);
    if (len == 0 || len > 253 || cap < len + 18) return -1;
    memset(out, 0, cap);
    out[0] = id >> 8; out[1] = id & 255;
    out[2] = 1; out[5] = 1; /* Recursion desired, one question. */
    for (i = 0; i <= len; ++i) {
        if (host[i] == '.' || i == len) {
            size_t label = i - start;
            if (!label || label > 63) return -1;
            out[n++] = (unsigned char)label;
            memcpy(out + n, host + start, label); n += label;
            start = i + 1;
        }
    }
    out[n++] = 0; out[n++] = 0; out[n++] = 1; /* IN A. */
    out[n++] = 0; out[n++] = 1;
    return (int)n;
}

/* Skip a possibly compressed owner name without following pointers. */
static int skip_name(const unsigned char *p, size_t len, size_t *pos)
{
    size_t steps = 0;
    while (*pos < len && ++steps <= 128) {
        unsigned int c = p[(*pos)++];
        if (c == 0) return 1;
        if ((c & 0xc0) == 0xc0) {
            unsigned int offset;
            if (*pos >= len) return 0;
            offset = ((c & 63) << 8) | p[(*pos)++];
            return offset < len;
        }
        if (c > 63 || c > len - *pos) return 0;
        *pos += c;
    }
    return 0;
}

int nd_dns_answer(const unsigned char *p, size_t len, unsigned short id,
                  const char *host, unsigned char address[4], int *has_address)
{
    unsigned char question[272];
    int qlen;
    unsigned int i, answers;
    size_t pos;
    *has_address = 0;
    if (len < 12 || u16(p) != id || !(p[2] & 0x80) || (p[2] & 0x78) || u16(p+4) != 1)
        return -1;
    if (p[2] & 2) return -2;
    qlen = nd_dns_query(question, sizeof(question), id, host);
    if (qlen < 0 || len < (size_t)qlen || memcmp(p+12, question+12, (size_t)qlen-12))
        return -1;
    if (p[3] & 15) return p[3] & 15;
    pos = (size_t)qlen; answers = u16(p+6);
    for (i = 0; i < answers; ++i) {
        unsigned int type, klass, bytes;
        if (!skip_name(p, len, &pos) || len - pos < 10) return -1;
        type = u16(p+pos); klass = u16(p+pos+2); bytes = u16(p+pos+8);
        pos += 10;
        if (bytes > len - pos) return -1;
        if (type == 1 && klass == 1 && bytes == 4 && !*has_address) {
            memcpy(address, p+pos, 4); *has_address = 1;
        }
        pos += bytes;
    }
    return 0;
}

static int prefix(const unsigned char *p, size_t len, const char *s)
{
    size_t i, n = strlen(s);
    if (len < n) return 0;
    for (i=0; i<n; ++i) {
        unsigned char c = p[i];
        if (c >= 'A' && c <= 'Z') c += 'a'-'A';
        if (c != (unsigned char)s[i]) return 0;
    }
    return 1;
}

int nd_http_status(const unsigned char *p, size_t len, size_t *body,
                   int *content_length, int *transfer_encoding)
{
    size_t end, line, next;
    int status;
    *body = 0; *content_length = -1; *transfer_encoding = 0;
    if (len < 12) return 0;
    if (memcmp(p,"HTTP/1.",7) || (p[7] != '0' && p[7] != '1') || p[8] != ' ' ||
        p[9] < '1' || p[9] > '5' || p[10] < '0' || p[10] > '9' ||
        p[11] < '0' || p[11] > '9' || (len > 12 && p[12] != ' ' && p[12] != '\r'))
        return -1;
    status = (p[9]-'0')*100 + (p[10]-'0')*10 + p[11]-'0';
    for (end = 0; end + 3 < len; ++end)
        if (!memcmp(p+end,"\r\n\r\n",4)) break;
    if (end + 3 >= len) return 0;
    *body = end + 4;
    for (line = 0; line < end; line = next+2) {
        for (next=line; next+1 < *body; ++next)
            if (p[next]=='\r' && p[next+1]=='\n') break;
        if (prefix(p+line, next-line, "content-length:")) {
            size_t j = line+15;
            int value=0, digits=0;
            if (*content_length >= 0) return -1;
            while (j < next && (p[j]==' ' || p[j]=='\t')) ++j;
            while (j < next && p[j]>='0' && p[j]<='9') {
                if (value > (INT_MAX-(p[j]-'0'))/10) return -1;
                value=value*10+p[j++]-'0'; ++digits;
            }
            while (j < next && (p[j]==' ' || p[j]=='\t')) ++j;
            if (!digits || j != next) return -1;
            *content_length=value;
        }
        if (prefix(p+line, next-line, "transfer-encoding:")) *transfer_encoding=1;
    }
    return status;
}
