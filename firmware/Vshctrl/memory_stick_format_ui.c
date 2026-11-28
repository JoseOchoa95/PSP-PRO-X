/* GPL-3.0-or-later */
#include "memory_stick_format_ui.h"

enum { ACCEPTED = 0, BACK = -1, JOB_COMPLETE = 0, MESSAGE_WITH_GUIDE = 2, PROGRESS = 3 };

int prox_format_ui_open(void *owner, const struct ProxPafWide *message, int icon,
                        ProxFormatCallback result, int options, int flags, void *settings,
                        struct ProxFormatUi *ui)
{
    if (ui->progress || result != ui->open_warning || !message || !message->text ||
        message->length >= sizeof(ui->original_text) / sizeof(ui->original_text[0]))
        return PROX_FORMAT_NO_ACCESS;
    unsigned length = message->length;
    for (unsigned i = 0; i < length; ++i)
        ui->original_text[i] = message->text[i];
    ui->original_text[length] = 0;
    ui->original.text = ui->original_text;
    ui->original.length = length;

    int token = ui->request(PROX_FORMAT_QUEUE, ui->cookie, 0);
    if (token < 1)
        return ui->error(owner, PROX_FORMAT_NO_ACCESS, ui->after_error, 0);
    ui->session = (unsigned)token;
    ui->settings = settings;
    ui->owner = owner;
    ui->icon = icon;
    ui->options = options;
    ui->flags = flags;
    ui->progress = ui->dialog(owner, &ui->checking, PROGRESS, 0, 0, 0, 0);
    if (ui->progress > 0 && ui->schedule(0, ui->scan, ui, -1, -1, ui->scanned))
        return ui->progress;

    if (ui->progress > 0)
        ui->close_dialog(ui->progress);
    ui->progress = 0;
    ui->request(PROX_FORMAT_DISCARD, ui->cookie, ui->session);
    ui->session = 0;
    return ui->error(owner, PROX_FORMAT_NO_ACCESS, ui->after_error, 0);
}

int prox_format_ui_scan(void *data)
{
    struct ProxFormatUi *ui = data;
    return ui->request(PROX_FORMAT_SCAN, ui->cookie, ui->session);
}

void prox_format_ui_scanned(int status, void *data, void *job, int result)
{
    (void)job;
    struct ProxFormatUi *ui = data;
    if (ui->progress <= 0)
        return;
    ui->close_dialog(ui->progress);
    ui->progress = 0;

    if (status == JOB_COMPLETE && result > 1 && ((unsigned)result >> 1) == ui->session) {
        if (result & 1) {
            if (ui->body.text && ui->body.length &&
                ui->dialog(ui->owner, &ui->body, MESSAGE_WITH_GUIDE, ui->preview_callback, 1, 0,
                           (void *)(uintptr_t)ui->session) > 0)
                return;
        } else if (ui->dialog(ui->owner, &ui->original, ui->icon, ui->open_warning,
                              ui->options, ui->flags, ui->settings) > 0) {
            return;
        }
    }
    ui->request(PROX_FORMAT_DISCARD, ui->cookie, ui->session);
    ui->session = 0;
    ui->error(ui->owner, PROX_FORMAT_NO_ACCESS, ui->after_error, 0);
}

int prox_format_ui_warning(void *owner, const struct ProxPafWide *message, int icon,
                           ProxFormatCallback result, int options, int flags, void *settings,
                           struct ProxFormatUi *ui)
{
    if (!ui->session || settings != ui->settings || result != ui->finish_warning)
        return ui->error(owner, PROX_FORMAT_NO_ACCESS, ui->after_error, 0);
    int warning = ui->dialog(owner, message, icon, ui->warning_callback, options, flags,
                             (void *)(uintptr_t)ui->session);
    if (warning > 0)
        return warning;
    ui->request(PROX_FORMAT_DISCARD, ui->cookie, ui->session);
    ui->session = 0;
    return ui->error(owner, PROX_FORMAT_NO_ACCESS, ui->after_error, 0);
}

void prox_format_ui_preview_result(int choice, void *session, struct ProxFormatUi *ui)
{
    unsigned generation = (unsigned)(uintptr_t)session;
    if (!generation || generation != ui->session)
        return;
    if (choice == ACCEPTED) {
        if (!ui->request(PROX_FORMAT_ADVANCE, ui->cookie, generation)) {
            ui->open_warning(ACCEPTED, ui->settings);
            return;
        }
        ui->error(ui->owner, PROX_FORMAT_UNAVAILABLE, ui->after_error, 0);
    } else {
        ui->request(PROX_FORMAT_DISCARD, ui->cookie, generation);
        ui->open_warning(BACK, ui->settings);
    }
    ui->session = 0;
}

void prox_format_ui_warning_result(int choice, void *session, struct ProxFormatUi *ui)
{
    unsigned generation = (unsigned)(uintptr_t)session;
    if (!generation || generation != ui->session)
        return;
    if (choice == ACCEPTED) {
        if (!ui->request(PROX_FORMAT_APPROVE, ui->cookie, generation)) {
            ui->finish_warning(ACCEPTED, ui->settings);
            return;
        }
        ui->error(ui->owner, PROX_FORMAT_UNAVAILABLE, ui->after_error, 0);
    } else {
        ui->request(PROX_FORMAT_DISCARD, ui->cookie, generation);
        ui->finish_warning(choice, ui->settings);
    }
    ui->session = 0;
}

int prox_format_ui_run(struct ProxFormatUi *ui)
{
    unsigned generation = ui->session;
    if (!generation)
        return PROX_FORMAT_NO_ACCESS;
    ui->session = 0;
    int status = ui->request(PROX_FORMAT_RUN, ui->cookie, generation);
    return status == PROX_FORMAT_NATIVE ? ui->native_worker() : status;
}
