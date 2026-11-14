/* PRO-X diagnostic protocol helpers, GPL-3.0-or-later. */
#ifndef PROX_NETDIAG_PROTOCOL_H
#define PROX_NETDIAG_PROTOCOL_H
#include <stddef.h>
int nd_dns_query(unsigned char *out, size_t capacity, unsigned short id, const char *host);
/* >= 0: DNS RCODE; 0 with has_address=0 is a valid reply without an A answer.
 * -1: malformed/mismatched; -2: truncated response (TCP retry not performed). */
int nd_dns_answer(const unsigned char *data, size_t length, unsigned short id,
                  const char *host, unsigned char address[4], int *has_address);
/* 0: incomplete, -1: malformed, otherwise HTTP status. */
int nd_http_status(const unsigned char *data, size_t length, size_t *body_offset,
                   int *content_length, int *transfer_encoding);
#endif
