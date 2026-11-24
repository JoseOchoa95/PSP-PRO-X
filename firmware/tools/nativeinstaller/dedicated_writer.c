#include "dedicated_writer.h"
#include <stdio.h>
#include <string.h>

int prox_dedicated_writer_check(const ProxDedicatedJob *job, unsigned int lease, unsigned int generation)
{
    static const char suffix_prefix[] = "/.dedicated-";
    char suffix[sizeof(suffix_prefix) + 17];
    if (!job || !lease || !generation || !memchr(job->install.root, 0, sizeof(job->install.root)))
        return -1;
    const char *leaf = strrchr(job->install.root, '/');
    snprintf(suffix, sizeof(suffix), "%s%08x-%08x", suffix_prefix, lease, generation);
    if (!leaf || strcmp(leaf, suffix))
        return -1;
    return prox_dedicated_verify(job);
}

#if defined(PROX_BACKGROUND_ENGINE) || defined(PROX_NATIVE_FLASH_TEST)
int prox_dedicated_writer_prepare(const ProxDedicatedJob *job, unsigned int lease, unsigned int generation,
                                  ProxNativeFlash *transaction)
{
    if (prox_dedicated_writer_check(job, lease, generation))
        return -1;
    return prox_native_flash_prepare_dedicated(&job->install, &job->worker, transaction);
}

int prox_dedicated_writer_finalize(const ProxDedicatedJob *job, ProxNativeFlash *transaction,
                                   const char *active_build)
{
    if (!job)
        return -1;
    return prox_native_flash_finalize_dedicated(&job->install, transaction, active_build);
}
#endif
