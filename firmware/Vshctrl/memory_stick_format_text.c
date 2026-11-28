/* GPL-3.0-or-later */
#include "memory_stick_format_text.h"

struct FormatLanguage {
    const uint16_t *checking, *intro, *capacity, *partitions, *partition, *limit;
    uint16_t decimal;
};

/* Sony 6.61 system language order. The title and button guide come from VSH. */
static const struct FormatLanguage languages[] = {
    {u"メモリースティックを確認中", u"すべてのデータが削除されます。", u"カード容量", u"パーティション", u"パーティション", u"上限", '.'},
    {u"Checking the Memory Stick", u"All data will be deleted.", u"Capacity", u"Partitions", u"Partition", u"Limit", '.'},
    {u"Vérification du Memory Stick", u"Toutes les données seront effacées.", u"Capacité", u"Partitions", u"Partition", u"Limite", ','},
    {u"Comprobando la Memory Stick", u"Se borrarán todos los datos.", u"Capacidad", u"Particiones", u"Partición", u"Límite", ','},
    {u"Memory Stick wird geprüft", u"Alle Daten werden gelöscht.", u"Kapazität", u"Partitionen", u"Partition", u"Limit", ','},
    {u"Verifica della Memory Stick", u"Tutti i dati verranno eliminati.", u"Capacità", u"Partizioni", u"Partizione", u"Limite", ','},
    {u"Memory Stick controleren", u"Alle gegevens worden gewist.", u"Capaciteit", u"Partities", u"Partitie", u"Limiet", ','},
    {u"A verificar o Memory Stick", u"Todos os dados serão apagados.", u"Capacidade", u"Partições", u"Partição", u"Limite", ','},
    {u"Проверка Memory Stick", u"Все данные будут удалены.", u"Ёмкость", u"Разделы", u"Раздел", u"Предел", ','},
    {u"Memory Stick 확인 중", u"모든 데이터가 삭제됩니다.", u"용량", u"파티션", u"파티션", u"한도", '.'},
    {u"正在檢查 Memory Stick", u"所有資料將被刪除。", u"容量", u"分割區", u"分割區", u"上限", '.'},
    {u"正在检查 Memory Stick", u"所有数据都将被删除。", u"容量", u"分区", u"分区", u"上限", '.'},
};

struct TextWriter {
    uint16_t *text;
    unsigned used, capacity;
};

static void append(struct TextWriter *writer, const uint16_t *value)
{
    while (*value && writer->used < writer->capacity)
        writer->text[writer->used++] = *value++;
}

static void whole_number(struct TextWriter *writer, unsigned value)
{
    uint16_t digits[11] = {0};
    unsigned start = 10;
    do {
        digits[--start] = '0' + value % 10;
        value /= 10;
    } while (value);
    append(writer, digits + start);
}

static void gib(struct TextWriter *writer, uint64_t sectors, uint16_t separator)
{
    enum { SECTORS_PER_GIB = 2097152 };
    unsigned size = (sectors * 100 + SECTORS_PER_GIB / 2) / SECTORS_PER_GIB;
    uint16_t fraction[] = {separator, '0' + size / 10 % 10, '0' + size % 10, 0};
    whole_number(writer, size / 100);
    append(writer, fraction);
    append(writer, u" GiB");
}

static const struct FormatLanguage *format_language(unsigned language)
{
    return &languages[language < sizeof(languages) / sizeof(languages[0]) ? language : 1];
}

unsigned prox_format_checking_text(unsigned language, uint16_t text[PROX_FORMAT_CHECKING_UNITS])
{
    struct TextWriter writer = {text, 0, PROX_FORMAT_CHECKING_UNITS - 1};
    append(&writer, format_language(language)->checking);
    append(&writer, u"...");
    text[writer.used] = 0;
    return writer.used;
}

unsigned prox_format_text(const struct ProxFormatReview *review, unsigned language,
                          uint16_t *text, unsigned capacity)
{
    if (!text || !capacity)
        return 0;
    text[0] = 0;
    if (review->screen != PROX_FORMAT_PREVIEW)
        return 0;

    const struct FormatLanguage *message = format_language(language);
    const struct MsFormatPlan *plan = &review->preview.plan;
    struct TextWriter writer = {text, 0, capacity};
    append(&writer, message->intro);
    append(&writer, u"\n\n");
    append(&writer, message->capacity);
    append(&writer, u": ");
    gib(&writer, review->preview.media.verified_sectors, message->decimal);
    append(&writer, u"\n");
    append(&writer, message->partitions);
    append(&writer, u" FAT32: ");
    whole_number(&writer, plan->volume_count);
    append(&writer, u"\n");
    append(&writer, message->limit);
    append(&writer, u": 1 TB\n\n");
    for (unsigned index = 0; index < plan->volume_count && writer.used < capacity; ++index) {
        struct MsFormatVolume volume;
        if (!prox_format_review_row(review, index, &volume)) {
            text[0] = 0;
            return 0;
        }
        append(&writer, message->partition);
        append(&writer, u" ");
        whole_number(&writer, index + 1);
        append(&writer, u": ");
        gib(&writer, volume.sectors, message->decimal);
        append(&writer, u"\n");
    }
    if (writer.used == capacity) {
        text[0] = 0;
        return 0;
    }
    text[writer.used] = 0;
    return writer.used;
}
