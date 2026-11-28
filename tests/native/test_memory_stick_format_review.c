/* GPL-3.0-or-later */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "memory_stick_format_review.h"

static struct MsFormatPreview example_preview(void)
{
    struct MsFormatPreview preview = {
        .media = {.reported_sectors = 163840000, .verified_sectors = 969932800},
    };
    assert(ms_format_layout(preview.media.reported_sectors, preview.media.verified_sectors,
                            &preview.plan) == MS_FORMAT_EXTENDED);
    assert(preview.plan.volume_count == 6);
    return preview;
}

int main(void)
{
    struct ProxFormatReview review;
    struct MsFormatPreview preview = example_preview(), confirmed;
    struct MsFormatVolume row, expected;

    assert(!prox_format_review_open(&review, 1, NULL));
    assert(!prox_format_review_take(&review, 1, 1, &confirmed));
    assert(prox_format_review_open(&review, 1, &preview));
    assert(!prox_format_review_take(&review, 1, 1, &confirmed));
    assert(!confirmed.plan.volume_count);

    assert(prox_format_review_open(&review, 1, &preview));
    assert(prox_format_review_row(&review, 0, &row) && row.sectors == preview.plan.full_sectors);
    assert(prox_format_review_row(&review, 5, &row));
    assert(ms_format_volume(&preview.plan, 5, &expected));
    assert(row.sectors == preview.plan.final_sectors && row.start == expected.start);
    assert(!prox_format_review_row(&review, 6, &row));
    prox_format_review_cancel(&review);
    assert(!prox_format_review_take(&review, 1, 1, &confirmed));

    assert(prox_format_review_open(&review, 1, &preview));
    assert(!prox_format_review_continue(&review, 1, 0));
    assert(!prox_format_review_continue(&review, 1, 1));
    assert(prox_format_review_open(&review, 1, &preview));
    assert(prox_format_review_continue(&review, 1, 1));
    assert(!prox_format_review_take(&review, 1, 0, &confirmed));
    assert(!prox_format_review_take(&review, 1, 1, &confirmed));

    assert(prox_format_review_open(&review, 1, &preview));
    assert(prox_format_review_continue(&review, 1, 1));
    prox_format_review_cancel(&review);
    assert(!prox_format_review_take(&review, 1, 1, &confirmed));

    assert(prox_format_review_open(&review, 1, &preview));
    assert(prox_format_review_continue(&review, 1, 1));
    assert(!prox_format_review_row(&review, 0, &row));
    assert(!prox_format_review_take(&review, 2, 1, &confirmed));
    assert(prox_format_review_take(&review, 1, 1, &confirmed));
    assert(confirmed.plan.usable_sectors == preview.plan.usable_sectors);
    assert(confirmed.plan.final_sectors == preview.plan.final_sectors);
    assert(!prox_format_review_take(&review, 1, 1, &confirmed));
    puts("PASS: format plan rows, stale generations, cancellation, and one-shot final acceptance");
    return 0;
}
