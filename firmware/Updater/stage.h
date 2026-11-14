/* Shared storage-only verification, used before installer writes. */
#ifndef PROX_STAGE_H
#define PROX_STAGE_H
#include "contract.h"
int prox_file_check(const char *path, unsigned int size, const char *sha256, int pbp);
int prox_zip_stage(const char *zip, const char *root, const ProxManifest *manifest);
/* Root must be a newly created private directory; no recursive deletion. */
int prox_stage_dirs(const char *root);
void prox_stage_remove(const char *root);
int prox_stage_check(const char *root, const ProxManifest *manifest);
/* Called by the installer only after the updater process has terminated. */
int prox_storage_prepare(const char *root, const char *game, const ProxManifest *manifest);
int prox_storage_commit(const char *game, const ProxManifest *manifest);
void prox_storage_discard(const char *game, const ProxManifest *manifest);
#endif
