/* GPL-3.0-or-later */
#ifndef MSSWITCH_AUTO_FORMAT_H
#define MSSWITCH_AUTO_FORMAT_H

#include "job.h"

/* Use from one worker thread that services the insertion callback from scan
   through cancellation or completion. The Settings scan is read-only. */
enum MsFormatLayoutResult ms_auto_format_scan(struct MsFormatPreview *preview);
int ms_auto_format_current(void);
void ms_auto_format_discard(void);
struct MsFormatResult ms_auto_format_execute(struct MsFormatPreview *confirmed, struct Fat32Ram *image);

#endif
