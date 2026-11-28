/* GPL-3.0-or-later */
#include <pspkernel.h>
#include <pspsdk.h>
#include <pspsysmem_kernel.h>
#include <pspthreadman_kernel.h>
#include <psputility_sysparam.h>
#include <string.h>

#include "auto_format.h"
#include "dirs_card.h"
#include "memory_stick_format_text.h"
#include "memory_stick_format_ui.h"
#include "memory_stick_format_ui_image.h"
#include "systemctrl.h"
#include "utils.h"

enum {
    OPEN_DIALOG = 0x2e70, FINAL_DIALOG = 0x1a404, NATIVE_DIALOG = 0x2a0a0,
    FORMAT_WORKER = 0x1a004, OPEN_WARNING = 0x1a38c, FINISH_WARNING = 0x1a25c,
    FORMAT_ERROR = 0x23db8, EXIT_ERROR = 0x19adc,
    SCHEDULE_TASK = 0x29ac8, CLOSE_DIALOG = 0x2a150,
    STOCK_SCHEDULE_CALL = 0x1e3f0, STOCK_CLOSE_CALL = 0x1a24c,
    VSH_USER_LEVEL = 4,
};

enum SessionStage { IDLE, QUEUED, ORDINARY, EXTENDED, WARNING, APPROVED_ORDINARY, APPROVED_EXTENDED };

struct UserFormat {
    struct ProxFormatUi ui;
    uint32_t open_thunk[4], warning_thunk[4], preview_result_thunk[4], warning_result_thunk[4];
    uint32_t run_thunk[4], stock_worker[4], request[2];
    uint32_t image[(sizeof(format_ui_code) + 3) / 4];
    uint16_t checking[PROX_FORMAT_CHECKING_UNITS];
    uint16_t text[PROX_FORMAT_TEXT_UNITS];
};

static struct UserFormat *user;
static unsigned cookie, generation;
static SceUID settings_id = -1;
static uintptr_t settings_text;
static SceUID request_sema = -1, response_sema = -1;
static int busy, worker_fault;
static struct {
    enum SessionStage stage;
    SceUID interface_thread;
    unsigned action, token;
    int result;
    struct ProxFormatReview review;
} session;
static struct Fat32Ram scratch;

static void discard(void)
{
    session.stage = IDLE;
    prox_format_review_cancel(&session.review);
    ms_auto_format_discard();
    dirs_card_stop_monitor();
}

static unsigned system_language(void)
{
    typedef int (*GetParam)(int, int *);
    GetParam get = (void *)sctrlHENFindFunction("sceUtility_Driver", "sceUtility", 0xa5da2406);
    int language = 1;
    if (get && get(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &language) < 0)
        language = 1;
    return language;
}

static int queue_scan(void)
{
    discard();
    unsigned length = prox_format_checking_text(system_language(), user->checking);
    user->ui.checking = (struct ProxPafWide){user->checking, length};
    user->ui.body.text = user->text;
    user->ui.body.length = 0;
    generation = (generation + 1) & 0x3fffffffu;
    if (!generation)
        generation = 1;
    session.stage = QUEUED;
    return generation;
}

static int scan(void)
{
    struct MsFormatPreview preview;
    enum MsFormatLayoutResult type = ms_auto_format_scan(&preview);
    if (type == MS_FORMAT_INVALID) {
        discard();
        return PROX_FORMAT_NO_ACCESS;
    }

    if (type == MS_FORMAT_STOCK) {
        session.stage = ORDINARY;
        return (int)(generation << 1);
    }
    if (prox_format_review_open(&session.review, generation, &preview)) {
        unsigned chars = prox_format_text(&session.review, system_language(), user->text, PROX_FORMAT_TEXT_UNITS);
        if (chars) {
            user->ui.body.length = chars;
            session.stage = EXTENDED;
            return (int)((generation << 1) | 1);
        }
    }
    discard();
    return PROX_FORMAT_NO_ACCESS;
}

static int format_extended(struct MsFormatPreview *confirmed)
{
    typedef int (*PowerGate)(int);
    PowerGate lock = (void *)sctrlHENFindFunction("scePower_Service", "scePower_driver", 0xd6d016ef);
    PowerGate unlock = (void *)sctrlHENFindFunction("scePower_Service", "scePower_driver", 0xca3d34c1);
    if (!lock || !unlock || lock(0) < 0)
        return PROX_FORMAT_NO_ACCESS;
    struct MsFormatResult result = ms_auto_format_execute(confirmed, &scratch);
    if (unlock(0) < 0)
        return PROX_FORMAT_NO_ACCESS;
    return result.failure == MS_FORMAT_COMPLETE ? 0 : PROX_FORMAT_NO_ACCESS;
}

static int dispatch(unsigned action, unsigned token)
{
    if (action == PROX_FORMAT_QUEUE)
        return queue_scan();
    if (session.stage == IDLE || token != generation)
        return PROX_FORMAT_NO_ACCESS;
    if (action == PROX_FORMAT_DISCARD) {
        discard();
        return 0;
    }
    if (action == PROX_FORMAT_SCAN && session.stage == QUEUED)
        return scan();
    if (!ms_auto_format_current()) {
        discard();
        return PROX_FORMAT_UNAVAILABLE;
    }
    if (action == PROX_FORMAT_ADVANCE && session.stage == EXTENDED &&
        prox_format_review_continue(&session.review, generation, 1)) {
        session.stage = WARNING;
        return 0;
    }
    if (action == PROX_FORMAT_APPROVE) {
        if (session.stage == ORDINARY) {
            session.stage = APPROVED_ORDINARY;
            return 0;
        }
        if (session.stage == WARNING) {
            session.stage = APPROVED_EXTENDED;
            return 0;
        }
    }
    if (action == PROX_FORMAT_RUN) {
        if (session.stage == APPROVED_ORDINARY) {
            discard();
            return PROX_FORMAT_NATIVE;
        }
        struct MsFormatPreview confirmed;
        if (session.stage == APPROVED_EXTENDED &&
            prox_format_review_take(&session.review, generation, 1, &confirmed)) {
            session.stage = IDLE;
            int result = format_extended(&confirmed);
            discard();
            return result;
        }
    }
    discard();
    return PROX_FORMAT_NO_ACCESS;
}

static int worker(SceSize size, void *arg)
{
    (void)size;
    (void)arg;
    while (!sceKernelWaitSemaCB(request_sema, 1, NULL)) {
        session.result = dispatch(session.action, session.token);
        sceKernelSignalSema(response_sema, 1);
    }
    discard();
    return 0;
}

static int prepare_worker(void)
{
    if (request_sema >= 0)
        return 1;
    request_sema = sceKernelCreateSema("PRO-X format requests", 0, 0, 1, NULL);
    response_sema = sceKernelCreateSema("PRO-X format results", 0, 0, 1, NULL);
    if (request_sema >= 0 && response_sema >= 0) {
        SceUID thread = sceKernelCreateThread("PRO-X format worker", worker, 30, 0x8000, 0, NULL);
        if (thread >= 0) {
            if (!sceKernelStartThread(thread, 0, NULL))
                return 1;
            sceKernelDeleteThread(thread);
        }
    }
    if (request_sema >= 0)
        sceKernelDeleteSema(request_sema);
    if (response_sema >= 0)
        sceKernelDeleteSema(response_sema);
    request_sema = response_sema = -1;
    return 0;
}

int proxNativeFormatRequest(unsigned action, unsigned key, unsigned token)
{
    if (action > PROX_FORMAT_RUN || sceKernelGetUserLevel() != VSH_USER_LEVEL)
        return PROX_FORMAT_NO_ACCESS;
    int k1 = pspSdkSetK1(0);
    int result = PROX_FORMAT_NO_ACCESS;
    SceModule2 *mod = (SceModule2 *)(uintptr_t)sceKernelFindModuleByName("sysconf_plugin_module");
    SceUID thread = sceKernelGetThreadId();
    unsigned irq = pspSdkDisableInterrupts();
    int admitted = !busy && !worker_fault && user && key == cookie && mod && mod->modid == settings_id &&
                   mod->text_addr == settings_text &&
                   (action == PROX_FORMAT_QUEUE || action == PROX_FORMAT_SCAN || action == PROX_FORMAT_RUN ||
                    action == PROX_FORMAT_DISCARD || thread == session.interface_thread);
    if (admitted)
        busy = 1;
    pspSdkEnableInterrupts(irq);
    if (admitted) {
        if (prepare_worker()) {
            if (action == PROX_FORMAT_QUEUE)
                session.interface_thread = thread;
            session.action = action;
            session.token = token;
            if (!sceKernelSignalSema(request_sema, 1) && !sceKernelWaitSema(response_sema, 1, NULL))
                result = session.result;
            else
                worker_fault = 1;
        }
        busy = 0;
    }
    pspSdkSetK1(k1);
    return result;
}

static int contains(const SceModule2 *mod, uintptr_t address, unsigned length)
{
    if (mod->nsegment > 4)
        return 0;
    for (unsigned i = 0; i < mod->nsegment; ++i) {
        uintptr_t start = mod->segmentaddr[i];
        if (address >= start && length <= mod->segmentsize[i] && address - start <= mod->segmentsize[i] - length)
            return 1;
    }
    return 0;
}

static struct UserFormat *allocate_ui(void)
{
    typedef int (*Alloc)(int, const char *, int, unsigned int, void *);
    typedef void *(*Head)(int);
    typedef int (*Free)(int);
    Alloc allocate = (void *)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0x237dbd4f);
    Head head = (void *)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0x9d9a5ba1);
    Free release = (void *)sctrlHENFindFunction("sceSystemMemoryManager", "SysMemForKernel", 0xb6d61d02);
    if (!allocate || !head || !release)
        return NULL;
    int block = allocate(2, "PRO-X format UI", PSP_SMEM_High, sizeof(struct UserFormat), NULL);
    if (block < 0)
        return NULL;
    uintptr_t address = (uintptr_t)head(block);
    if ((address & 3) || address < 0x08800000u || address > 0x0c000000u - sizeof(struct UserFormat)) {
        release(block);
        return NULL;
    }
    return (struct UserFormat *)address;
}

static void thunk(uint32_t *out, unsigned argument, uintptr_t target)
{
    uintptr_t context = (uintptr_t)&user->ui;
    out[0] = 0x3c000000u | (argument << 16) | (context >> 16);
    out[1] = 0x34000000u | (argument << 21) | (argument << 16) | (context & 0xffff);
    out[2] = MAKE_JUMP(target);
    out[3] = 0;
}

void prox_patch_native_format(SceModule2 *mod)
{
    uintptr_t base = mod->text_addr;
    uint32_t *text = (void *)base;
    if (busy || (settings_id == mod->modid && settings_text == base) ||
        base < 0x08800000u || base > 0x0c000000u - CLOSE_DIALOG - 8 ||
        mod->text_size < FORMAT_ERROR + 8 || !contains(mod, base + NATIVE_DIALOG, 8) ||
        !contains(mod, base + SCHEDULE_TASK, 8) || !contains(mod, base + CLOSE_DIALOG, 8))
        return;
    if (text[OPEN_DIALOG / 4] != MAKE_CALL(base + NATIVE_DIALOG) || text[OPEN_DIALOG / 4 + 1] ||
        text[FINAL_DIALOG / 4] != MAKE_CALL(base + NATIVE_DIALOG) || text[FINAL_DIALOG / 4 + 1] != 0x24090001 ||
        text[FORMAT_WORKER / 4] != 0x27bdffd0 || text[FORMAT_WORKER / 4 + 1] != 0xafbf0024 ||
        text[OPEN_WARNING / 4] != 0x27bdffc0 || text[FINISH_WARNING / 4] != 0x27bdffe0 ||
        text[STOCK_SCHEDULE_CALL / 4] != MAKE_CALL(base + SCHEDULE_TASK) ||
        text[STOCK_CLOSE_CALL / 4] != MAKE_CALL(base + CLOSE_DIALOG))
        return;

    unsigned call = sctrlKernelQuerySystemCall(proxNativeFormatRequest);
    if (call > 0xfffff)
        return;

    if (!user)
        user = allocate_ui();
    if (!user)
        return;

    uintptr_t image = (uintptr_t)user->image;
    memcpy(user->image, format_ui_code, sizeof(format_ui_code));
    thunk(user->open_thunk, 11, image + FORMAT_UI_OPEN);
    thunk(user->warning_thunk, 11, image + FORMAT_UI_WARNING);
    thunk(user->preview_result_thunk, 6, image + FORMAT_UI_PREVIEW_RESULT);
    thunk(user->warning_result_thunk, 6, image + FORMAT_UI_WARNING_RESULT);
    thunk(user->run_thunk, 4, image + FORMAT_UI_RUN);
    user->stock_worker[0] = text[FORMAT_WORKER / 4];
    user->stock_worker[1] = text[FORMAT_WORKER / 4 + 1];
    user->stock_worker[2] = MAKE_JUMP(base + FORMAT_WORKER + 8);
    user->stock_worker[3] = 0;
    user->request[0] = 0x03e00008u;
    user->request[1] = MAKE_SYSCALL(call);
    if (++cookie == 0)
        ++cookie;
    user->ui = (struct ProxFormatUi){
        .request = (void *)user->request,
        .dialog = (void *)(base + NATIVE_DIALOG), .schedule = (void *)(base + SCHEDULE_TASK),
        .close_dialog = (void *)(base + CLOSE_DIALOG), .error = (void *)(base + FORMAT_ERROR),
        .after_error = (void *)(base + EXIT_ERROR), .open_warning = (void *)(base + OPEN_WARNING),
        .finish_warning = (void *)(base + FINISH_WARNING), .native_worker = (void *)user->stock_worker,
        .preview_callback = (void *)user->preview_result_thunk, .warning_callback = (void *)user->warning_result_thunk,
        .scan = (void *)(image + FORMAT_UI_SCAN), .scanned = (void *)(image + FORMAT_UI_SCANNED), .cookie = cookie,
    };
    settings_id = mod->modid;
    settings_text = base;
    text[FORMAT_WORKER / 4] = MAKE_JUMP(user->run_thunk);
    text[FORMAT_WORKER / 4 + 1] = 0;
    text[OPEN_DIALOG / 4] = MAKE_CALL(user->open_thunk);
    text[FINAL_DIALOG / 4] = MAKE_CALL(user->warning_thunk);
    sync_cache();
}
