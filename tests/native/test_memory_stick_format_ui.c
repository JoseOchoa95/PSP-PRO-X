/* GPL-3.0-or-later */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "memory_stick_format_text.h"
#include "memory_stick_format_ui.h"

enum { COOKIE = 41, GENERATION = 53 };
static struct ProxFormatUi ui;
static int scan_result, action_result, worker_result;
static unsigned actions[PROX_FORMAT_RUN + 1], panels, dialogs, errors, workers, finishes, warnings, closing;
static unsigned owner, settings;
static void *callback_data;
static ProxFormatCallback callback;
static ProxFormatTask run_scan;
static ProxFormatTaskDone done_scan;
static void *scan_data;
static int schedule_ok = 1, job_status;
static uint16_t body[PROX_FORMAT_TEXT_UNITS];
static uint16_t checking[PROX_FORMAT_CHECKING_UNITS];

static int request(unsigned action, unsigned key, unsigned session)
{
    assert(key == COOKIE && action <= PROX_FORMAT_RUN);
    assert(session == (action == PROX_FORMAT_QUEUE ? 0 : GENERATION));
    ++actions[action];
    if (action == PROX_FORMAT_QUEUE)
        return GENERATION;
    if (action == PROX_FORMAT_SCAN)
        return scan_result;
    return action == PROX_FORMAT_RUN ? worker_result : action_result;
}

static int dialog(void *context, const struct ProxPafWide *message, int icon,
                  ProxFormatCallback handler, int options, int flags, void *data)
{
    assert(context == &owner && message);
    if (icon == 3) {
        assert(!handler && !options && !flags && !data);
        assert(message->text == checking && message->length > 20);
        return 203;
    }
    callback = handler;
    callback_data = data;
    if (icon == 2) {
        assert(options == 1 && !flags);
        assert(message->text == body && message->length > 100);
        ++panels;
        return 201;
    }
    assert(!icon && flags == 1);
    assert(options == 0 || options == 4);
    if (!options)
        assert(message->text == ui.original_text && message->length == 7 && message->text[0] == 'F');
    ++dialogs;
    return 200;
}

static void *schedule(int queue, ProxFormatTask run, void *data, int priority, int options,
                      ProxFormatTaskDone done)
{
    assert(!queue && run == prox_format_ui_scan && data == &ui && priority == -1 && options == -1);
    assert(done == prox_format_ui_scanned && ui.progress == 203);
    if (!schedule_ok)
        return NULL;
    run_scan = run;
    done_scan = done;
    scan_data = data;
    return &ui;
}

static void close_dialog(int handle)
{
    assert(handle == 203);
    ++closing;
}

static int error(void *context, int code, ProxFormatCallback handler, void *data)
{
    assert(context == &owner && handler == ui.after_error && !data);
    assert(code == PROX_FORMAT_UNAVAILABLE || code == PROX_FORMAT_NO_ACCESS);
    ++errors;
    return 202;
}

static void finish_warning(int choice, void *data)
{
    assert((choice == 0 || choice == -1) && data == &settings);
    ++finishes;
}

static void open_warning(int choice, void *data)
{
    assert((choice == 0 || choice == -1) && data == &settings);
    ++warnings;
    if (!choice) {
        struct ProxPafWide stock = {u"Are you sure?", 13};
        assert(prox_format_ui_warning(&owner, &stock, 0, finish_warning, 4, 1, data, &ui) == 200);
    }
}

static void preview_callback(int choice, void *data)
{
    prox_format_ui_preview_result(choice, data, &ui);
}

static void warning_callback(int choice, void *data)
{
    prox_format_ui_warning_result(choice, data, &ui);
}

static int stock_worker(void)
{
    ++workers;
    return 15;
}

static void begin(int scan)
{
    scan_result = scan;
    uint16_t prompt[] = u"Format?";
    struct ProxPafWide stock = {prompt, 7};
    unsigned scans = actions[PROX_FORMAT_SCAN];
    assert(prox_format_ui_open(&owner, &stock, 0, open_warning, 0, 1, &settings, &ui) == (schedule_ok ? 203 : 202));
    assert(actions[PROX_FORMAT_SCAN] == scans);
    for (unsigned i = 0; i < 7; ++i)
        prompt[i] = 0;
    if (schedule_ok) {
        unsigned closes = closing;
        int result = job_status ? scan_result : run_scan(scan_data);
        assert(closing == closes && ui.progress == 203);
        done_scan(job_status, scan_data, &run_scan, result);
        assert(closing == closes + 1 && !ui.progress);
    }
}

static int contains(const uint16_t *text, const uint16_t *needle)
{
    for (; *text; ++text) {
        unsigned i = 0;
        while (needle[i] && text[i] == needle[i])
            ++i;
        if (!needle[i])
            return 1;
    }
    return 0;
}

int main(void)
{
    struct ProxFormatReview review;
    struct MsFormatPreview preview = {
        .media = {.reported_sectors = 163840000, .verified_sectors = 969932800},
    };
    assert(ms_format_layout(preview.media.reported_sectors, preview.media.verified_sectors,
                           &preview.plan) == MS_FORMAT_EXTENDED);
    assert(prox_format_review_open(&review, GENERATION, &preview));
    unsigned chars = prox_format_text(&review, 3, body, PROX_FORMAT_TEXT_UNITS);
    assert(chars > 100 && !body[chars]);
    assert(contains(body, u"Capacidad: 462,50 GiB"));
    assert(contains(body, u"Particiones FAT32: 6") && contains(body, u"Partición 6:") && contains(body, u"1 TB"));
    uint16_t short_text[8];
    assert(!prox_format_text(&review, 3, short_text, 8) && !short_text[0]);
    unsigned progress_chars = prox_format_checking_text(3, checking);
    assert(progress_chars > 20 && contains(checking, u"Comprobando la Memory Stick..."));

    ui = (struct ProxFormatUi){
        .request = request, .dialog = dialog, .schedule = schedule, .close_dialog = close_dialog,
        .error = error, .after_error = finish_warning, .open_warning = open_warning,
        .finish_warning = finish_warning, .native_worker = stock_worker, .preview_callback = preview_callback,
        .warning_callback = warning_callback, .cookie = COOKIE,
        .body = {body, chars}, .checking = {checking, progress_chars},
        .scan = prox_format_ui_scan, .scanned = prox_format_ui_scanned,
    };

    begin(PROX_FORMAT_NO_ACCESS);
    assert(!dialogs && !panels && errors == 1);
    begin(GENERATION * 2 + 1);
    assert(panels == 1 && callback == preview_callback);
    callback(-1, callback_data);
    assert(actions[PROX_FORMAT_DISCARD] == 2 && !dialogs && !actions[PROX_FORMAT_RUN] && !ui.session);

    begin(GENERATION * 2 + 1);
    callback(0, (void *)(uintptr_t)(GENERATION - 1));
    assert(!actions[PROX_FORMAT_ADVANCE]);
    callback(0, callback_data);
    assert(actions[PROX_FORMAT_ADVANCE] == 1 && dialogs == 1 && callback == warning_callback);
    assert(!actions[PROX_FORMAT_APPROVE] && !actions[PROX_FORMAT_RUN]);
    callback(-1, callback_data);
    assert(actions[PROX_FORMAT_DISCARD] == 3 && finishes == 1 && !ui.session);

    begin(GENERATION * 2 + 1);
    callback(0, callback_data);
    action_result = PROX_FORMAT_UNAVAILABLE;
    callback(0, callback_data);
    assert(!ui.session && errors == 2 && finishes == 1 && !actions[PROX_FORMAT_RUN]);
    action_result = 0;

    begin(GENERATION * 2 + 1);
    callback(0, callback_data);
    callback(0, callback_data);
    assert(ui.session == GENERATION && !actions[PROX_FORMAT_RUN]);
    assert(!prox_format_ui_run(&ui) && !workers && actions[PROX_FORMAT_RUN] == 1);
    assert(prox_format_ui_run(&ui) == PROX_FORMAT_NO_ACCESS && actions[PROX_FORMAT_RUN] == 1);

    begin(GENERATION * 2);
    assert(callback == open_warning);
    callback(0, callback_data);
    callback(0, callback_data);
    worker_result = PROX_FORMAT_NATIVE;
    assert(prox_format_ui_run(&ui) == 15 && workers == 1);

    job_status = 1;
    begin(GENERATION * 2 + 1);
    assert(errors == 3 && panels == 4 && !ui.session);

    schedule_ok = 0;
    begin(GENERATION * 2 + 1);
    assert(errors == 4 && !ui.progress && !ui.session && actions[PROX_FORMAT_DISCARD] == 5);
    puts("PASS: nonblocking scan dialog, native message layout, scheduling failure, cancellation and stock continuation");
    return 0;
}
