/* GPL-3.0-or-later */
#ifndef PROX_MEMORY_STICK_FORMAT_UI_H
#define PROX_MEMORY_STICK_FORMAT_UI_H

#include <stdint.h>

enum ProxFormatAction {
    PROX_FORMAT_QUEUE,
    PROX_FORMAT_SCAN,
    PROX_FORMAT_ADVANCE,
    PROX_FORMAT_APPROVE,
    PROX_FORMAT_DISCARD,
    PROX_FORMAT_RUN,
};

enum {
    PROX_FORMAT_NATIVE = 1,
    PROX_FORMAT_NO_ACCESS = (int)0x80108326u,
    PROX_FORMAT_UNAVAILABLE = (int)0x8010835du,
};

struct ProxPafWide {
    const uint16_t *text;
    unsigned length;
};

typedef void (*ProxFormatCallback)(int choice, void *data);
typedef int (*ProxFormatDialog)(void *owner, const struct ProxPafWide *message, int icon,
                                ProxFormatCallback result, int options, int flags, void *data);
typedef int (*ProxFormatTask)(void *data);
typedef void (*ProxFormatTaskDone)(int status, void *data, void *job, int result);
typedef void *(*ProxFormatSchedule)(int queue, ProxFormatTask run, void *data,
                                    int priority, int options, ProxFormatTaskDone done);

struct ProxFormatUi {
    int (*request)(unsigned action, unsigned cookie, unsigned session);
    ProxFormatDialog dialog;
    ProxFormatSchedule schedule;
    void (*close_dialog)(int dialog);
    int (*error)(void *owner, int code, ProxFormatCallback result, void *data);
    ProxFormatCallback after_error;
    ProxFormatCallback open_warning, finish_warning;
    int (*native_worker)(void);
    ProxFormatCallback preview_callback, warning_callback;
    ProxFormatTask scan;
    ProxFormatTaskDone scanned;
    unsigned cookie, session;
    void *settings, *owner;
    struct ProxPafWide checking, body, original;
    uint16_t original_text[128];
    int progress, icon, options, flags;
};

/* Each entry is invoked by a user-space thunk that sets the final argument. */
int prox_format_ui_open(void *owner, const struct ProxPafWide *message, int icon,
                        ProxFormatCallback result, int options, int flags, void *settings,
                        struct ProxFormatUi *ui);
int prox_format_ui_warning(void *owner, const struct ProxPafWide *message, int icon,
                           ProxFormatCallback result, int options, int flags, void *settings,
                           struct ProxFormatUi *ui);
void prox_format_ui_preview_result(int choice, void *session, struct ProxFormatUi *ui);
void prox_format_ui_warning_result(int choice, void *session, struct ProxFormatUi *ui);
int prox_format_ui_run(struct ProxFormatUi *ui);
int prox_format_ui_scan(void *data);
void prox_format_ui_scanned(int status, void *data, void *job, int result);

#endif
