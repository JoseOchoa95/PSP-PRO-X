#include "dedicated_update.h"
#include "native_payload.h"
#include "payload_fixture.h"
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PROX_DEDICATED_GENERATED
#include "expected_update.h"
#endif

#define SECTION 60
#define INDEX (64 + 48 * PROX_DEDICATED_PARTS)
#define TOTAL (SECTION + INDEX + 512 * PROX_DEDICATED_PARTS)

static unsigned char pbp[TOTAL + 1];
static unsigned char original[TOTAL];
static ProxDedicatedExpected expected = {
    .size = TOTAL, .running = {0, 3, 4}, .target = {0, 4, 0},
    .build_id = "pro-x-cafebabecafe", .devkit = 0x06060110u, .model = 1,
    .engine_revision = 1, .minimum_engine_revision = 1
};

static void part_hash(unsigned int i)
{
    unsigned char *entry = pbp + SECTION + 64 + i * 48;
    assert(!mbedtls_sha256(pbp + SECTION + INDEX + i * 512, 512, entry + 16, 0));
}

static void pin(void)
{
    assert(!mbedtls_sha256(pbp, expected.size, expected.sha256, 0));
}

static void make_pbp(void)
{
    memcpy(pbp, "\0PBP", 4);
    w(pbp + 4, 0x10000);
    w(pbp + 8, 40);
    for (unsigned int i = 1; i < 8; ++i)
        w(pbp + 8 + i * 4, SECTION);
    memcpy(pbp + 40, "\0PSF", 4);
    w(pbp + 44, 0x101);
    unsigned char *header = pbp + SECTION;
    memcpy(header, "PROXUPD\0", 8);
    w(header + 8, 1);
    w(header + 12, INDEX);
    w(header + 16, expected.devkit);
    w(header + 20, 2);
    w(header + 24, 1);
    w(header + 32, 4);
    memcpy(header + 40, expected.build_id, sizeof(expected.build_id));
    w(header + 60, PROX_DEDICATED_PARTS);
    for (unsigned int i = 0; i < PROX_DEDICATED_PARTS; ++i) {
        unsigned char *entry = header + 64 + i * 48;
        unsigned int offset = INDEX + i * 512;
        w(entry, i);
        w(entry + 4, offset);
        w(entry + 8, 512);
        w(entry + 12, 512);
        image(header + offset, 512, i == PROX_DEDICATED_WORKER ? "ProxUpdater" : prox_native_payload_names[i]);
        part_hash(i);
    }
    memcpy(original, pbp, TOTAL);
    pin();
}

static void rejected(int repin)
{
    ProxDedicatedUpdate out, before;
    memset(&out, 0xa5, sizeof(out));
    before = out;
    if (repin)
        pin();
    assert(prox_dedicated_update(pbp, expected.size, &expected, &out) < 0);
    assert(!memcmp(&out, &before, sizeof(out)));
    expected.size = TOTAL;
    memcpy(pbp, original, TOTAL);
    pin();
}

#ifdef PROX_DEDICATED_GENERATED
static void verify_generated(const char *path)
{
    ProxDedicatedUpdate update;
    unsigned char *data = malloc(fixture.size);
    FILE *file = fopen(path, "rb");
    assert(data && file && fread(data, 1, fixture.size, file) == fixture.size);
    assert(fgetc(file) == EOF && !ferror(file) && !fclose(file));
    assert(!prox_dedicated_update(data, fixture.size, &fixture, &update));
    free(data);
    printf("PASS: C consumer verified the compiled dedicated PBP (%u bytes, %u parts)\n",
           fixture.size, PROX_DEDICATED_PARTS);
}
#endif

int main(int argc, char **argv)
{
    ProxDedicatedUpdate out;
    make_pbp();
    assert(!prox_dedicated_update(pbp, TOTAL, &expected, &out));
    assert(out.parts[PROX_DEDICATED_WORKER].size == 512);
    assert(out.parts[PROX_DEDICATED_WORKER].offset == TOTAL - 512);

    pbp[TOTAL - 1] = 1;
    rejected(0); /* The external digest rejects tampering. */
    pbp[TOTAL - 1] = 1;
    rejected(1); /* An authenticated header must also bind every part. */
    image(pbp + TOTAL - 512, 512, "WrongWorker");
    part_hash(PROX_DEDICATED_WORKER);
    rejected(1);
    w(pbp + 32, SECTION - 1); /* Hidden executable DATA.PSP. */
    rejected(1);
    w(pbp + 36, 0xffffffff);
    rejected(1);
    w(pbp + SECTION + 8, 2); /* Unknown format. */
    rejected(1);
    w(pbp + SECTION + 16, 0x06060010);
    rejected(1);
    w(pbp + SECTION + 20, 1);
    rejected(1);
    w(pbp + SECTION + 24, 2);
    rejected(1);
    w(pbp + SECTION + 32, 5);
    rejected(1);
    pbp[SECTION + 57] = 'b'; /* A different otherwise valid build. */
    rejected(1);
    w(pbp + SECTION + 60, PROX_NI_PARTS);
    rejected(1);
    w(pbp + SECTION + 64 + 48, 0); /* Duplicate module. */
    rejected(1);
    w(pbp + SECTION + 68, INDEX + 1); /* Gap in the inventory. */
    rejected(1);
    w(pbp + SECTION + 72, 0xffffffff);
    rejected(1);
    unsigned char zipped[512];
    unsigned int length = compressed(zipped, sizeof(zipped), pbp + SECTION + INDEX, 512);
    memcpy(pbp + SECTION + INDEX, zipped, length);
    w(pbp + SECTION + 72, length); /* Stored and expanded lengths differ. */
    rejected(1);
    expected.size = TOTAL - 1;
    rejected(1);
    expected.size = TOTAL + 1;
    rejected(1);
    assert(prox_dedicated_update(pbp, PROX_DEDICATED_MAX_BYTES + 1u, &expected, &out) < 0);
    expected.model = 0;
    rejected(0);
    expected.model = 1;
    expected.running[1] = 4;
    rejected(0);
    expected.running[1] = 5;
    rejected(0);
    puts("PASS: dedicated EBOOT bounds, strict identity, integrity and complete part inventory");
#ifdef PROX_DEDICATED_GENERATED
    assert(argc == 2);
    verify_generated(argv[1]);
#else
    (void)argc;
    (void)argv;
#endif
}
