/* GPL-3.0-or-later */
#ifndef PROX_MEMORY_STICK_FORMAT_TEXT_H
#define PROX_MEMORY_STICK_FORMAT_TEXT_H

#include "memory_stick_format_review.h"

/* 1 TB divided into volumes of at least 1 GiB plus a final remainder. */
enum { PROX_FORMAT_TEXT_UNITS = 1024 * 36 + 256, PROX_FORMAT_CHECKING_UNITS = 64 };

unsigned prox_format_checking_text(unsigned language, uint16_t text[PROX_FORMAT_CHECKING_UNITS]);
unsigned prox_format_text(const struct ProxFormatReview *review, unsigned language,
                          uint16_t *text, unsigned capacity);

#endif
