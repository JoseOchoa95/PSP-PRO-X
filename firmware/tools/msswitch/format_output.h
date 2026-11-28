/* GPL-3.0-or-later */
#ifndef MSSWITCH_FORMAT_OUTPUT_H
#define MSSWITCH_FORMAT_OUTPUT_H

#ifdef PROX_FORMAT_RESIDENT
#include <stdio.h>
/* Keep type checking and diagnostics in the standalone build. */
#define format_trace(...) ((void)sizeof(printf(__VA_ARGS__)))
#else
#include <pspdebug.h>
#define format_trace pspDebugScreenPrintf
#endif

#endif
