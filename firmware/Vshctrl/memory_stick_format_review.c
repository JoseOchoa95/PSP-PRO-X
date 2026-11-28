/* GPL-3.0-or-later */
#include "memory_stick_format_review.h"

void prox_format_review_cancel(struct ProxFormatReview *review)
{
    *review = (struct ProxFormatReview){0};
}

int prox_format_review_open(struct ProxFormatReview *review, uint32_t generation,
                            const struct MsFormatPreview *preview)
{
    struct MsFormatVolume last;
    prox_format_review_cancel(review);
    if (!generation || !preview || !preview->plan.volume_count ||
        preview->plan.usable_sectors > preview->media.verified_sectors ||
        !ms_format_volume(&preview->plan, preview->plan.volume_count - 1, &last))
        return 0;

    review->preview = *preview;
    review->generation = generation;
    review->screen = PROX_FORMAT_PREVIEW;
    return 1;
}

int prox_format_review_row(const struct ProxFormatReview *review, unsigned index,
                           struct MsFormatVolume *volume)
{
    return review->screen == PROX_FORMAT_PREVIEW &&
           ms_format_volume(&review->preview.plan, index, volume);
}

int prox_format_review_continue(struct ProxFormatReview *review, uint32_t generation, int insertion_current)
{
    if (generation != review->generation)
        return 0;
    if (!insertion_current)
        prox_format_review_cancel(review);
    if (review->screen != PROX_FORMAT_PREVIEW)
        return 0;

    review->screen = PROX_FORMAT_WARNING;
    return 1;
}

int prox_format_review_take(struct ProxFormatReview *review, uint32_t generation, int insertion_current,
                            struct MsFormatPreview *confirmed)
{
    *confirmed = (struct MsFormatPreview){0};
    if (generation != review->generation)
        return 0;
    int accepted = insertion_current && review->screen == PROX_FORMAT_WARNING;
    if (accepted)
        *confirmed = review->preview;
    prox_format_review_cancel(review);
    return accepted;
}
