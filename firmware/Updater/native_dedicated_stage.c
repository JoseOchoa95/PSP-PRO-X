/* Exact consented EBOOT download and read-back, without a flash writer. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bridge.h"
#include "dedicated_job.h"
#include "native_dedicated_stage.h"
#include "policy.h"
#include "prox_product.h"
#include "transport.h"
#include "update.h"

typedef struct {
    char *bytes;
    size_t size, limit;
} ManifestBuffer;

static int save_manifest(const unsigned char *bytes, size_t size, void *context)
{
    ManifestBuffer *buffer = context;
    if (size > buffer->limit - buffer->size)
        return -1;
    memcpy(buffer->bytes + buffer->size, bytes, size);
    buffer->size += size;
    return 0;
}

static void hex(const unsigned char digest[32], char out[65])
{
    for (unsigned int i = 0; i < 32; ++i)
        snprintf(out + 2 * i, 3, "%02x", digest[i]);
}

static int same_manifest(const ProxCatalogOffer *offer, const char *ca, const unsigned int running[3],
                         unsigned int model)
{
    unsigned int size = offer->release.manifest.size;
    if (!size || size > 32768 || !prox_hash_text(offer->manifest_sha256))
        return -1;
    ManifestBuffer buffer = {.bytes = malloc(size), .limit = size};
    PjToken *tokens = calloc(8192, sizeof(*tokens));
    ProxManifest manifest;
    ProxResponse response = {0};
    Pj json;
    char hash[65];
    int result = -1;
    if (!buffer.bytes || !tokens ||
        prox_https(offer->release.manifest.url, ca, size, save_manifest, &buffer, &response) ||
        response.status != 200 || buffer.size != size || response.bytes != size || prox_stopping())
        goto done;
    hex(response.sha256, hash);
    if (!strcmp(hash, offer->manifest_sha256) &&
        (!offer->release.manifest.digest[0] || !strcmp(hash, offer->release.manifest.digest)) &&
        !pj_parse(&json, buffer.bytes, buffer.size, tokens, 8192) &&
        !prox_manifest(&json, &offer->release, running, running, model, &manifest) &&
        !strcmp(manifest.version, offer->manifest.version) &&
        !strcmp(manifest.build_id, offer->manifest.build_id) &&
        !strcmp(manifest.package_sha256, offer->manifest.package_sha256) &&
        manifest.package_size == offer->manifest.package_size &&
        manifest.minimum_engine_revision == offer->manifest.minimum_engine_revision)
        result = 0;
done:
    free(tokens);
    free(buffer.bytes);
    return result;
}

typedef struct {
    SceUID file;
    unsigned int received, total;
    ProxNativeProgress progress;
} PackageWriter;

static int save_package(const unsigned char *bytes, size_t size, void *context)
{
    PackageWriter *writer = context;
    if (size > writer->total - writer->received || prox_stopping() ||
        sceIoWrite(writer->file, bytes, size) != (int)size)
        return -1;
    writer->received += (unsigned int)size;
    return writer->progress ? writer->progress(writer->received, writer->total, NULL) : 0;
}

#ifndef PROX_NATIVE_DEDICATED_OTA
static int fixture_transfer(const char *directory, PackageWriter *writer)
{
    unsigned char block[8192];
    char path[96];
    snprintf(path, sizeof(path), "%s/PROX-DEDICATED-TEST.PBP", directory);
    FILE *input = fopen(path, "rb");
    if (!input)
        return -1;
    int result = -1;
    size_t size;
    while ((size = fread(block, 1, sizeof(block), input)) != 0) {
        if (save_package(block, size, writer))
            goto done;
        sceKernelDelayThread(200000);
    }
    if (!ferror(input))
        result = 0;
done:
    return fclose(input) ? -1 : result;
}
#endif

static int prepare_staged(const char *root, const ProxCatalogOffer *offer,
                         const unsigned int running[3], unsigned int model, ProxDedicatedJob *job)
{
    const ProxManifest *manifest = &offer->manifest;
    ProxDedicatedExpected expected = {.size = manifest->package_size, .devkit = 0x06060110u, .model = model,
                                      .engine_revision = PROX_DEDICATED_ENGINE_REVISION,
                                      .minimum_engine_revision = manifest->minimum_engine_revision};
    if (!prox_hash_text(manifest->package_sha256))
        return -1;
    for (unsigned int i = 0; i < sizeof(expected.sha256); ++i) {
        char pair[] = {manifest->package_sha256[2 * i], manifest->package_sha256[2 * i + 1], 0};
        expected.sha256[i] = (unsigned char)strtoul(pair, NULL, 16);
    }
    memcpy(expected.build_id, manifest->build_id, sizeof(expected.build_id));
    memcpy(expected.running, running, sizeof(expected.running));
    memcpy(expected.target, manifest->numbers, sizeof(expected.target));
    if (prox_dedicated_prepare(root, &expected, offer->manifest_sha256, job))
        return -1;
    prox_log("Dedicated install job prepared: %s -> %s, build %s, worker %u bytes",
             job->install.source_version, job->install.target_version, job->install.build_id, job->worker.size);
    return 0;
}

int prox_native_dedicated_discard(const ProxDedicatedJob *job)
{
    char path[256];
    if (prox_dedicated_remove_parts(job))
        return -1;
    snprintf(path, sizeof(path), "%s/EBOOT.PBP", job->install.root);
    if (sceIoRemove(path) < 0 || sceIoRmdir(job->install.root) < 0)
        return -1;
    prox_log("Dedicated diagnostic stage removed; no firmware write attempted");
    return 0;
}

int prox_native_dedicated_stage(const char *directory, const char *ca, const unsigned int running[3],
                                unsigned int model, const ProxCatalogOffer *offer, int fixture,
                                unsigned int lease, unsigned int generation, ProxNativeProgress progress,
                                ProxDedicatedJob *out)
{
    if (!directory || (strcmp(directory, "ms0:/PROXDIAG") && strcmp(directory, "ef0:/PROXDIAG")))
        return PROX_DEDICATED_STAGE_STORAGE;
#ifdef PROX_NATIVE_DEDICATED_OTA
    if(fixture)return -1;
#endif
    if (!offer || !out || !lease || !generation || offer->release.format != PROX_RELEASE_DEDICATED ||
        !offer->manifest.package_size || offer->manifest.package_size > PROX_DEDICATED_MAX_BYTES ||
        prox_stopping())
        return -1;
    if (!prox_power_ready())
        return PROX_DEDICATED_STAGE_POWER;
    /* Keep the EBOOT and all its extracted files simultaneously. Reserve
     * up to 64 KiB each for allocation rounding and directory growth. */
    if (prox_storage_space(directory, 2ull * offer->manifest.package_size + 16u * 65536u))
        return PROX_DEDICATED_STAGE_STORAGE;
    if (!fixture && same_manifest(offer, ca, running, model))
        return -1;
    if (prox_stopping() || proxUpdateGetState(0) != 1 || proxUpdateGetState(4) != 0x06060110 ||
        proxUpdateGetState(5) != (int)model)
        return -1;
    for (unsigned int i = 0; i < 3; ++i) {
        if (proxUpdateGetState(i + 1) != (int)running[i])
            return -1;
    }

    char stage[96], path[112];
    snprintf(stage, sizeof(stage), "%s/.dedicated-%08x-%08x", directory, lease, generation);
    if (sceIoMkdir(stage, 0700) < 0)
        return PROX_DEDICATED_STAGE_STORAGE;
    snprintf(path, sizeof(path), "%s/EBOOT.PBP", stage);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_EXCL | PSP_O_CREAT, 0600);
    int result = PROX_DEDICATED_STAGE_STORAGE;
    if (fd >= 0) {
        PackageWriter writer = {.file = fd, .total = offer->manifest.package_size, .progress = progress};
#ifndef PROX_NATIVE_DEDICATED_OTA
        if (fixture) {
            result = fixture_transfer(directory, &writer);
        } else
#endif
        {
            ProxResponse response = {0};
            result = prox_https(offer->release.package.url, ca, writer.total, save_package, &writer, &response);
            if (result || response.status != 200 || response.bytes != writer.total)
                result = -1;
        }
        if (sceIoClose(fd) < 0 || writer.received != writer.total || prox_stopping())
            result = -1;
        if (!result) {
            result = prepare_staged(stage, offer, running, model, out);
            prox_log("Dedicated EBOOT verification and job preparation: %s (%u bytes, %u parts)", result ? "FAIL" : "PASS",
                     writer.received, (unsigned int)PROX_DEDICATED_PARTS);
            if (!result)
                return 0;
        }
        if (sceIoRemove(path) < 0)
            result = PROX_DEDICATED_STAGE_STORAGE;
    }
    if (sceIoRmdir(stage) < 0)
        result = PROX_DEDICATED_STAGE_STORAGE;
    return result;
}
