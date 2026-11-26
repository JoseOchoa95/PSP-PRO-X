/* GPL-3.0-or-later */
#ifndef MSSWITCH_DIRS_CARD_H
#define MSSWITCH_DIRS_CARD_H

struct Fat32Ram;
int dirs_card_load_reference(void);
int dirs_card_preview(void);
void dirs_card_write(struct Fat32Ram *image);
int dirs_card_cleanup(void);

#endif
