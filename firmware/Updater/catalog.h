/* Authenticated query only: no stage, package, power or installer side effects. */
#ifndef PROX_CATALOG_H
#define PROX_CATALOG_H
#include "contract.h"
typedef struct {
    ProxRelease release; ProxManifest manifest;
    char manifest_sha256[65];
    unsigned int pages,candidates,rejected;
} ProxCatalogOffer;
/* 0: complete validated catalog with no newer target, 1: compatible newer target,
 * -1: error/cancel/invalid contract. Never turn a failed query into 0. */
int prox_catalog_query(const char *ca,const unsigned int running[3],
                       unsigned int model,ProxCatalogOffer *offer);
#endif
