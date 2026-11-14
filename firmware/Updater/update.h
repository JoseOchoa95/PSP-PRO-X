#ifndef PROX_UPDATE_H
#define PROX_UPDATE_H
#include "contract.h"
typedef struct { char root[192], manifest_sha256[65], source_version[40]; } ProxJob;
int prox_prepare_update(const char *directory, const char *ca, const unsigned int running[3],
                        unsigned int model, ProxJob *job);
/* Platform operations are explicit so the production decision flow is testable
 * without network, a PSP filesystem or any flash installation capability. */
int prox_confirm(const char *prompt);
int prox_storage_space(const char *device, unsigned long long required);
int prox_new_stage(char root[192], const char *directory, const char *build_id);
int prox_power_ready(void);
int prox_launch_job(const ProxJob *job);
void prox_observe_install(const char *directory,const unsigned int running[3]);
#endif
