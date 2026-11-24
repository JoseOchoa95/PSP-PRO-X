/* PRO-X completion messages in PSP system language order. GPL-3.0-or-later. */
#include "native_completion_builtin.h"
#include "prox_completion_preview.h"

static const unsigned short *const messages[] = {
    u"PRO-Xの更新が完了しました。",
    u"PRO-X update complete.",
    u"PRO-X a été mis à jour.",
    u"PRO-X se actualizó correctamente.",
    u"PRO-X wurde aktualisiert.",
    u"PRO-X è stato aggiornato.",
    u"PRO-X is bijgewerkt.",
    u"PRO-X foi atualizado.",
    u"PRO-X успешно обновлён.",
    u"PRO-X 업데이트를 완료했습니다.",
    u"PRO-X 已成功更新。",
    u"PRO-X 已成功更新。",
};

int prox_completion_builtin_text(unsigned int language, unsigned short *out, unsigned int capacity)
{
    if (!out || language >= sizeof(messages) / sizeof(messages[0]))
        return -1;
    const unsigned short *message = messages[language];
    unsigned int units = 1;
    while (units <= PROX_COMPLETION_UNITS && message[units - 1])
        ++units;
    if (units > PROX_COMPLETION_UNITS || units > capacity)
        return -1;
    memcpy(out, message, units * sizeof(*out));
    return (int)units;
}
