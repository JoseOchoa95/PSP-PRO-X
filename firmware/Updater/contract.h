/* PRO-X update release/manifest contract. GPL-3.0-or-later. */
#ifndef PROX_UPDATE_CONTRACT_H
#define PROX_UPDATE_CONTRACT_H
#include "json.h"
#define PROX_UPDATE_MAX (16u*1024u*1024u)
#define PROX_UPDATE_FILES 8
typedef struct { char url[2048], digest[65]; unsigned int size; } ProxAsset;
typedef struct {
    char tag[40], version[40]; unsigned int numbers[3];
    ProxAsset manifest, package;
} ProxRelease;
typedef struct { unsigned int size; char sha256[65]; } ProxFile;
typedef struct {
    char version[40], build_id[32], summary[241], package_name[96];
    unsigned int numbers[3], models;
    unsigned int package_size;
    char package_sha256[65];
    ProxFile files[PROX_UPDATE_FILES];
} ProxManifest;
extern const char *const prox_update_names[PROX_UPDATE_FILES];
/* 1 candidate, 0 excluded by flags/version/missing contract, -1 invalid. */
int prox_release(const Pj *, int token, const unsigned int running[3], ProxRelease *);
int prox_manifest(const Pj *, const ProxRelease *, const unsigned int running[3],
                  const unsigned int client[3], unsigned int model, ProxManifest *);
int prox_hash_text(const char *);
#endif
