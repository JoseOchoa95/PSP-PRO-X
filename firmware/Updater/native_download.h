/* Native consent -> verified private stage. No UI or installer execution. */
#ifndef PROX_NATIVE_DOWNLOAD_H
#define PROX_NATIVE_DOWNLOAD_H
#include "catalog.h"
#include "update.h"
/* Called only after an affirmative native UI decision. Returns 0 only for a
 * complete verified stage. Any error/cancel removes owned partial staging. */
int prox_native_download(const char *directory,const char *ca,const unsigned int running[3],
                         unsigned int model,const ProxCatalogOffer *offer,ProxJob *job);
typedef int (*ProxNativeProgress)(unsigned int received,unsigned int total,void *context);
int prox_native_download_observed(const char *,const char *,const unsigned int [3],
                                  unsigned int,const ProxCatalogOffer *,ProxJob *,ProxNativeProgress,void *);
#endif
