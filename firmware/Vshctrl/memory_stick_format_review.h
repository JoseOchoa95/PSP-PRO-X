/* GPL-3.0-or-later */
#ifndef PROX_MEMORY_STICK_FORMAT_REVIEW_H
#define PROX_MEMORY_STICK_FORMAT_REVIEW_H

#include "../tools/msformat/job.h"

enum ProxFormatScreen {
    PROX_FORMAT_CLOSED,
    PROX_FORMAT_PREVIEW,
    PROX_FORMAT_WARNING,
};

struct ProxFormatReview {
    enum ProxFormatScreen screen;
    struct MsFormatPreview preview;
    uint32_t generation;
};

/* Start from the already verified plan for the current insertion. */
int prox_format_review_open(struct ProxFormatReview *review, uint32_t generation,
                            const struct MsFormatPreview *preview);
void prox_format_review_cancel(struct ProxFormatReview *review);
int prox_format_review_row(const struct ProxFormatReview *review, unsigned index,
                           struct MsFormatVolume *volume);
int prox_format_review_continue(struct ProxFormatReview *review, uint32_t generation, int insertion_current);
/* Consume the plan once, only from Sony's final acceptance callback. The
   formatter still revalidates the card and takes exclusive access. */
int prox_format_review_take(struct ProxFormatReview *review, uint32_t generation, int insertion_current,
                            struct MsFormatPreview *confirmed);

#endif
