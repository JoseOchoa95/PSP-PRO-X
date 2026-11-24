/* User-mode query worker entry point for the flash0 diagnostic. GPL-3.0-or-later. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "native_service.h"
#include "prox_resident_query.h"
#include "transport.h"

PSP_MODULE_INFO("ProxUpdater", 0, 0, 3);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(128);
PSP_HEAP_SIZE_KB(4096);

int prox_native_mode = 1;

int prox_stopping(void)
{
    return prox_native_is_cancelled();
}

void prox_log(const char *format, ...)
{
    char line[384];
    va_list arguments;

    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    prox_native_log_line(line);
}

int main(int argc, char **argv)
{
    if (argc != 3 || !argv || !argv[0] || !argv[1] ||
        strcmp(argv[0], PROX_RESIDENT_WORKER_ARG) || strcmp(argv[1], PROX_RESIDENT_QUERY_ARG))
        return -1;
    prox_native_worker(argc, argv);
    return -1;
}
