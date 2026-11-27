/* GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "card_snapshot.h"

enum { REPORTED = 232259584, PHYSICAL = 500695040, HIGH = 268435456, END_SAMPLE = 500694016 };

struct Card {
    uint32_t length, forbidden, first, alternate;
    int swapped, blank, mbr_changed, repeat_bytes;
};

static int read_card(void *context, uint32_t lba, uint8_t *out)
{
    const struct Card *card = context;
    assert(lba < card->length && lba != card->forbidden);
    memset(out, 0, FAT32_SECTOR_BYTES);
    if (!lba) {
        out[0] = card->mbr_changed ? 8 : 1;
    } else if (!card->blank) {
        if (lba == card->first)
            out[1] = card->swapped ? 8 : 3;
        else if (lba == card->alternate)
            out[1] = card->repeat_bytes ? 3 : 5;
    }
    return 1;
}

int main(void)
{
    struct MsFormatPlan plan;
    struct MsCardSnapshot snapshot;
    struct Card card = {PHYSICAL, 2048, HIGH, END_SAMPLE, 0, 0, 0, 0};
    assert(ms_format_layout(REPORTED, PHYSICAL, &plan) == MS_FORMAT_EXTENDED);
    assert(ms_card_capture(&plan, REPORTED, read_card, &card, &snapshot));
    assert(snapshot.witnesses[0].lba == HIGH && snapshot.witnesses[1].lba == END_SAMPLE);
    card.swapped = 1;
    assert(!ms_card_matches(&snapshot, 0, read_card, &card));
    card.swapped = 0;
    card.mbr_changed = 1;
    assert(ms_card_matches(&snapshot, 0, read_card, &card));
    assert(!ms_card_matches(&snapshot, 1, read_card, &card));
    card.blank = 1;
    assert(!ms_card_capture(&plan, REPORTED, read_card, &card, &snapshot));
    assert(!ms_card_matches(&snapshot, 1, read_card, &card));
    card.blank = 0;
    card.repeat_bytes = 1;
    assert(!ms_card_capture(&plan, REPORTED, read_card, &card, &snapshot));

    card.length = 1048576000;
    assert(ms_format_layout(243269632, card.length, &plan) == MS_FORMAT_EXTENDED);
    struct MsFormatVolume logical;
    assert(ms_format_volume(&plan, 3, &logical) && logical.ebr);
    card.forbidden = logical.ebr;
    card.first = logical.ebr + 1;
    card.alternate = card.length - 1024;
    card.repeat_bytes = 0;
    assert(ms_card_capture(&plan, logical.ebr, read_card, &card, &snapshot));
    assert(snapshot.witnesses[0].lba == card.first && snapshot.witnesses[1].lba == card.alternate);
    puts("PASS: live card witnesses, MBR changes, missing evidence, media swap and extended layout");
    return 0;
}
