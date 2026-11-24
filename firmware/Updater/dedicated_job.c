#include "dedicated_job.h"
#include "policy.h"
#include "prox_resident_slot.h"
#include "stage.h"
#include "transport.h"
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PROX_NATIVE_DEDICATED_PREVIEW
#include "prox_native_flash_paths.h"
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <mbedtls/sha256.h>
#endif

#define MS_STAGE_DIR "ms0:/PROXDIAG"
#define EF_STAGE_DIR "ef0:/PROXDIAG"

static void hash_text(const unsigned char digest[32], char out[65])
{
    for (unsigned int i = 0; i < 32; ++i)
        snprintf(out + i * 2, 3, "%02x", digest[i]);
}

static void part_path(char path[256], const char *root, unsigned int part)
{
    snprintf(path, 256, "%s/native/%02u.prx", root, part);
}

static const ProxNativePart *job_part(const ProxDedicatedJob *job, unsigned int part)
{
    return part == PROX_DEDICATED_WORKER ? &job->worker : &job->install.parts[part];
}

static int valid_root(const char *root)
{
    const size_t prefix = sizeof(MS_STAGE_DIR "/.dedicated-") - 1;
    if (strncmp(root, MS_STAGE_DIR "/.dedicated-", prefix) &&
        strncmp(root, EF_STAGE_DIR "/.dedicated-", prefix))
        return 0;
    const char *suffix = root + prefix;
    return strlen(suffix) == 17 && suffix[8] == '-' &&
           strspn(suffix, "0123456789abcdef") == 8 &&
           strspn(suffix + 9, "0123456789abcdef") == 8 &&
           strncmp(suffix, "00000000", 8) && strcmp(suffix + 9, "00000000");
}

static int valid_job(const ProxDedicatedJob *job)
{
    unsigned int running[3], target[3];
    if (!job || !memchr(job->install.root, 0, sizeof(job->install.root)) ||
        !valid_root(job->install.root) || job->install.model != 1 ||
        prox_resident_build_id(job->install.build_id) ||
        !memchr(job->install.source_version, 0, sizeof(job->install.source_version)) ||
        !memchr(job->install.target_version, 0, sizeof(job->install.target_version)) ||
        prox_version(job->install.source_version, running) || prox_version(job->install.target_version, target) ||
        prox_compare(target, running) <= 0 || job->install.manifest_sha256[64] ||
        !prox_hash_text(job->install.manifest_sha256) || !job->package.size ||
        job->package.size > PROX_DEDICATED_MAX_BYTES || job->package.sha256[64] ||
        !prox_hash_text(job->package.sha256))
        return 0;
    for (unsigned int part = 0; part < PROX_DEDICATED_PARTS; ++part) {
        const ProxNativePart *expected = job_part(job, part);
        if (!expected->size || expected->size > 1024u * 1024u || expected->sha256[64] ||
            !prox_hash_text(expected->sha256))
            return 0;
    }
    return 1;
}

int prox_dedicated_verify(const ProxDedicatedJob *job)
{
    char path[256];
    if (!valid_job(job) || prox_stopping())
        return -1;
    snprintf(path, sizeof(path), "%s/EBOOT.PBP", job->install.root);
    if (prox_file_check(path, job->package.size, job->package.sha256, 0))
        return -1;
    for (unsigned int part = 0; part < PROX_DEDICATED_PARTS; ++part) {
        const ProxNativePart *expected = job_part(job, part);
        part_path(path, job->install.root, part);
        if (prox_stopping() || prox_file_check(path, expected->size, expected->sha256, 0))
            return -1;
    }
    return prox_stopping() ? -1 : 0;
}

static int read_exact(const char *path, void *bytes, size_t size)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return -1;
    int complete = fread(bytes, 1, size, file) == size && fgetc(file) == EOF && !ferror(file);
    return fclose(file) || !complete ? -1 : 0;
}

#ifndef PROX_NATIVE_DEDICATED_PREVIEW
#define COMMITTED_NAME PROX_DEDICATED_COMMITTED_NAME
typedef struct {
    char magic[8];
    ProxDedicatedJob job;
    unsigned int prepared,backups,worker_prepared;
    ProxNativePart before[10];
    unsigned char digest[32];
} CommittedJob;
_Static_assert(sizeof(CommittedJob) == 2084, "Dedicated cleanup record v2 layout");
static const char committed_magic[8] = {'P', 'R', 'O', 'X', 'C', 'M', 'T', '2'};

/* A file missing during retry is already cleaned. A different file or I/O
 * error leaves the path alone and keeps the stage available for inspection. */
static int remove_verified(const char *path, const ProxNativePart *expected)
{
    struct stat info;
    if (stat(path, &info))
        return errno == ENOENT ? 0 : -1;
    if (!S_ISREG(info.st_mode) || prox_file_check(path, expected->size, expected->sha256, 0))
        return -1;
    return sceIoRemove(path) < 0 ? -1 : 0;
}

static int remove_empty(const char *path)
{
    struct stat info;
    if (stat(path, &info))
        return errno == ENOENT ? 0 : -1;
    return !S_ISDIR(info.st_mode) || sceIoRmdir(path) < 0 ? -1 : 0;
}

static int cleanup_card_files(const ProxDedicatedJob *job)
{
    char path[256];
    for (unsigned int part = 0; part < PROX_DEDICATED_PARTS; ++part) {
        part_path(path, job->install.root, part);
        if (remove_verified(path, job_part(job, part)))
            return -1;
    }
    snprintf(path, sizeof(path), "%s/native", job->install.root);
    if (remove_empty(path))
        return -1;
    snprintf(path, sizeof(path), "%s/EBOOT.PBP", job->install.root);
    if (remove_verified(path, &job->package))
        return -1;
    return remove_empty(job->install.root);
}

static int installed(const ProxDedicatedJob *job)
{
    char worker[PROX_RESIDENT_SLOT_BYTES];
    if (prox_resident_slot(job->install.build_id, worker) ||
        prox_file_check(worker, job->worker.size, job->worker.sha256, 0))
        return -1;
    for (unsigned int part = 0; part < PROX_NATIVE_FLASH_FILES; ++part) {
        const ProxNativePart *expected = job_part(job, part);
        if (prox_file_check(prox_native_flash_paths[part], expected->size, expected->sha256, 0))
            return -1;
    }
    return 0;
}

static int sync_card(const char *directory)
{
    char device[] = {directory[0], directory[1], directory[2], ':', 0};
    return sceIoSync(device, 0) < 0 ? -1 : 0;
}
/* The downloader owns this stage until the writer durably removes the
 * marker before opening flash0 for writes. An unmarked extracted stage can
 * belong to a failed installation from this or an earlier release. */
static const char download_marker[] = "PROX-DOWNLOAD-1";
#define DOWNLOAD_MARKER ".downloading"

static int file_type(const char *path)
{
    struct stat info;
    if (stat(path, &info))
        return errno == ENOENT ? 0 : -1;
    if (S_ISREG(info.st_mode)) return 1;
    if (S_ISDIR(info.st_mode)) return 2;
    return -1;
}

int prox_dedicated_mark_download(const char *root)
{
    char path[256];
    if (!root || !valid_root(root))
        return -1;
    snprintf(path, sizeof(path), "%s/" DOWNLOAD_MARKER, root);
    SceUID file = sceIoOpen(path, PSP_O_WRONLY | PSP_O_EXCL | PSP_O_CREAT, 0600);
    if (file < 0)
        return -1;
    int count = sceIoWrite(file, download_marker, sizeof(download_marker));
    if (sceIoClose(file) < 0 || count != (int)sizeof(download_marker) || sync_card(root)) {
        (void)sceIoRemove(path);
        return -1;
    }
    return 0;
}

int prox_dedicated_begin_write(const ProxDedicatedJob *job)
{
    char path[256], marker[sizeof(download_marker)];
    if (!valid_job(job))
        return -1;
    snprintf(path, sizeof(path), "%s/" DOWNLOAD_MARKER, job->install.root);
    if (file_type(path) != 1 || read_exact(path, marker, sizeof(marker)) ||
        memcmp(marker, download_marker, sizeof(marker)) || sceIoRemove(path) < 0)
        return -1;
    return sync_card(job->install.root);
}

static int remove_regular(const char *path)
{
    int type = file_type(path);
    return type == 0 ? 0 : type != 1 || sceIoRemove(path) < 0 ? -1 : 0;
}

/* Return 1 for a recovery stage. The caller must not erase it. */
static int discard_download(const char *root)
{
    char path[256], marker_path[256], marker[sizeof(download_marker)];
    if (!valid_root(root) || file_type(root) != 2)
        return 1;
    snprintf(marker_path, sizeof(marker_path), "%s/" DOWNLOAD_MARKER, root);
    int marker_type = file_type(marker_path);
    snprintf(path, sizeof(path), "%s/native", root);
    int native_type = file_type(path);
    if (marker_type == 0) {
        /* A legacy partial EBOOT could not reach the writer without native/. */
        if (native_type != 0)
            return 1;
    } else if (marker_type != 1 || read_exact(marker_path, marker, sizeof(marker)) ||
               memcmp(marker, download_marker, sizeof(marker))) {
        return 1;
    }
    if (native_type == 2) {
        for (unsigned int part = 0; part < PROX_DEDICATED_PARTS; ++part) {
            part_path(path, root, part);
            if (remove_regular(path))
                return -1;
        }
        snprintf(path, sizeof(path), "%s/native/engine.prx", root);
        if (remove_regular(path))
            return -1;
        snprintf(path, sizeof(path), "%s/native", root);
        if (remove_empty(path))
            return -1;
    } else if (native_type != 0) {
        return 1;
    }
    snprintf(path, sizeof(path), "%s/EBOOT.PBP", root);
    if (remove_regular(path) || remove_regular(marker_path) || remove_empty(root))
        return -1;
    return 0;
}

static int is_report(const char *name)
{
    static const char *const prefixes[] = {"native-report-", "native-download-", "native-cleanup-"};
    if (!strcmp(name, "dispatch.txt"))
        return 1;
    if (!strncmp(name, "native-install-pro-x-", 21)) {
        const char *suffix = name + 21;
        return strspn(suffix, "0123456789abcdef") == 12 && !strcmp(suffix + 12, ".txt");
    }
    for (unsigned int i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        size_t length = strlen(prefixes[i]);
        if (!strncmp(name, prefixes[i], length)) {
            const char *suffix = name + length;
            return strspn(suffix, "0123456789") == 3 && !strcmp(suffix + 3, ".txt");
        }
    }
    return 0;
}

static int purge_workspace(const char *directory, int before_download)
{
    int result = 0;
    /* Check for remaining recovery stages before removing any reports. */
    for (int reports = 0; reports < 2 && !result; ++reports) {
        DIR *entries = opendir(directory);
        if (!entries)
            return -1;
        for (;;) {
            errno = 0;
            struct dirent *entry = readdir(entries);
            if (!entry) {
                if (errno) result = -1;
                break;
            }
            char path[256];
            int length = snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
            if (length < 0 || length >= (int)sizeof(path))
                continue;
            if (!reports && valid_root(path)) {
                int stage = discard_download(path);
                if (stage < 0 || (stage > 0 && before_download)) {
                    result = -1;
                    break;
                }
            } else if (reports && is_report(entry->d_name) && remove_regular(path)) {
                result = -1;
                break;
            }
        }
        if (closedir(entries))
            return -1;
    }
    return result ? -1 : sync_card(directory);
}

int prox_dedicated_reset_download(const char *directory)
{
    if (prox_dedicated_pending(directory) != 0)
        return -1;
    return purge_workspace(directory, 1);
}

int prox_dedicated_discard_download(const char *root)
{
    return discard_download(root) == 0 ? 0 : -1;
}

int prox_dedicated_record_available(const ProxDedicatedJob *job)
{
    if(!valid_job(job))return -1;
    return prox_dedicated_pending(job->install.root[0]=='m'?MS_STAGE_DIR:EF_STAGE_DIR)==0 ? 0 : -1;
}
int prox_dedicated_pending(const char *directory)
{
    char path[96];struct stat state;
    if(!directory || (strcmp(directory,MS_STAGE_DIR) && strcmp(directory,EF_STAGE_DIR)))return -1;
    snprintf(path,sizeof(path),"%s/" COMMITTED_NAME,directory);
    if(stat(path,&state))return errno==ENOENT ? 0 : -1;
    return S_ISREG(state.st_mode) ? 1 : -1;
}

static int valid_sidecars(const CommittedJob *record)
{
    const unsigned int mask=(1u << PROX_NATIVE_FLASH_FILES)-1;
    if((record->prepared & ~mask) || (record->backups & ~record->prepared) || record->worker_prepared>1)
        return 0;
    for(unsigned int i=0;i<PROX_NATIVE_FLASH_FILES;++i) {
        const ProxNativePart *before=&record->before[i];
        if((record->backups & (1u<<i)) && (!before->size || before->size>PROX_UPDATE_MAX ||
           before->sha256[64] || !prox_hash_text(before->sha256)))return 0;
    }
    return 1;
}

int prox_dedicated_record_commit(const ProxDedicatedJob *job, const ProxNativeFlash *transaction)
{
    const unsigned int mask=(1u << PROX_NATIVE_FLASH_FILES)-1;
    if (!transaction || transaction->changed!=transaction->prepared || transaction->originals!=transaction->backups ||
        (transaction->prepared|transaction->identical)!=mask || transaction->worker_cleanup ||
        prox_dedicated_verify(job) || installed(job))
        return -1;
    CommittedJob record = {0};
    memcpy(record.magic, committed_magic, sizeof(record.magic));
    record.job = *job;
    record.prepared=transaction->prepared;record.backups=transaction->backups;
    record.worker_prepared=(unsigned int)transaction->worker_prepared;
    memcpy(record.before,transaction->before,sizeof(record.before));
    if (!valid_sidecars(&record) || mbedtls_sha256((const unsigned char *)&record, offsetof(CommittedJob, digest), record.digest, 0))
        return -1;
    char path[96];
    snprintf(path, sizeof(path), "%.*s/" COMMITTED_NAME, (int)sizeof(MS_STAGE_DIR) - 1, job->install.root);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_EXCL | PSP_O_CREAT, 0600);
    if (fd < 0)
        return -1;
    int written = sceIoWrite(fd, &record, sizeof(record));
    if (sceIoClose(fd) < 0 || written != (int)sizeof(record) || sync_card(job->install.root))
        return -1;
    CommittedJob readback;
    return read_exact(path, &readback, sizeof(readback)) || memcmp(&readback, &record, sizeof(record)) ? -1 : 0;
}

static int read_committed(const char *directory, const char *active_build, const char *active_version,
                          unsigned int model, CommittedJob *record)
{
    if (!directory || (strcmp(directory, MS_STAGE_DIR) && strcmp(directory, EF_STAGE_DIR)) ||
        !active_build || !active_version)
        return -1;
    unsigned char digest[32];
    char path[96];
    snprintf(path, sizeof(path), "%s/" COMMITTED_NAME, directory);
    if (read_exact(path, record, sizeof(*record)) || memcmp(record->magic, committed_magic, sizeof(record->magic)) ||
        mbedtls_sha256((const unsigned char *)record, offsetof(CommittedJob, digest), digest, 0) ||
        memcmp(record->digest, digest, sizeof(digest)) || !valid_job(&record->job) || !valid_sidecars(record) ||
        strncmp(directory, record->job.install.root, strlen(directory)) ||
        strcmp(record->job.install.build_id, active_build) || strcmp(record->job.install.target_version, active_version) ||
        record->job.install.model != model)
        return -1;
    return 0;
}

int prox_dedicated_read_committed(const char *directory, const char *active_build, const char *active_version,
                                  unsigned int model, ProxDedicatedJob *job, ProxNativeFlash *transaction,
                                  unsigned char digest[32])
{
    CommittedJob record;
    if(!job || !transaction || !digest || read_committed(directory,active_build,active_version,model,&record))return -1;
    ProxNativeFlash saved={0};
    if(prox_resident_slot(active_build,saved.worker_path))return -1;
    saved.prepared=record.prepared;saved.backups=record.backups;saved.originals=record.backups;
    saved.worker=record.job.worker;saved.worker_prepared=(int)record.worker_prepared;saved.worker_cleanup=1;
    memcpy(saved.before,record.before,sizeof(saved.before));
    memcpy(digest, record.digest, sizeof(record.digest));
    *job=record.job;*transaction=saved;
    return 0;
}

int prox_dedicated_cleanup_committed(const char *directory, const char *active_build, const char *active_version,
                                     unsigned int model, const unsigned char digest[32])
{
    CommittedJob record;
    char path[96];
    if(!digest || read_committed(directory,active_build,active_version,model,&record) ||
       memcmp(digest, record.digest, sizeof(record.digest)) || cleanup_card_files(&record.job) ||
       purge_workspace(directory, 0))return -1;
    snprintf(path,sizeof(path),"%s/" COMMITTED_NAME,directory);
    if(sync_card(directory) || sceIoRemove(path)<0 || sync_card(directory))return -1;
    /* Leave unfamiliar files in place. All updater reports were removed. */
    (void)sceIoRmdir(directory);
    return 0;
}
#endif

static int remove_parts(const char *root, unsigned int owned)
{
    char path[256];
    int failed = 0;
    for (unsigned int part = 0; part < PROX_DEDICATED_PARTS; ++part) {
        if (!(owned & (1u << part)))
            continue;
        part_path(path, root, part);
        if (sceIoRemove(path) < 0)
            failed = 1;
    }
    snprintf(path, sizeof(path), "%s/native", root);
    if (sceIoRmdir(path) < 0)
        failed = 1;
    return failed ? -1 : 0;
}

int prox_dedicated_remove_parts(const ProxDedicatedJob *job)
{
    if (!job || !memchr(job->install.root, 0, sizeof(job->install.root)) || !valid_root(job->install.root))
        return -1;
    return remove_parts(job->install.root, (1u << PROX_DEDICATED_PARTS) - 1u);
}

static int read_package(const char *root, unsigned char *bytes, unsigned int size)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/EBOOT.PBP", root);
    return read_exact(path, bytes, size);
}

static int write_parts(const char *root, const unsigned char *bytes, const ProxDedicatedUpdate *parsed,
                       ProxDedicatedJob *job)
{
    char path[256];
    unsigned int owned = 0;
    snprintf(path, sizeof(path), "%s/native", root);
    if (sceIoMkdir(path, 0700) < 0)
        return -1;

    for (unsigned int part = 0; part < PROX_DEDICATED_PARTS; ++part) {
        const ProxDedicatedPart *info = &parsed->parts[part];
        ProxNativePart *target = part == PROX_DEDICATED_WORKER ? &job->worker : &job->install.parts[part];
        if (prox_stopping())
            goto fail;
        target->size = info->size;
        hash_text(info->sha256, target->sha256);
        part_path(path, root, part);
        SceUID file = sceIoOpen(path, PSP_O_WRONLY | PSP_O_EXCL | PSP_O_CREAT, 0600);
        if (file < 0)
            goto fail;
        owned |= 1u << part;
        int count = sceIoWrite(file, bytes + info->offset, info->size);
        if (sceIoClose(file) < 0 || count != (int)info->size ||
            prox_file_check(path, target->size, target->sha256, 0))
            goto fail;
    }
    if (!prox_stopping())
        return 0;
fail:
    if (remove_parts(root, owned))
        prox_log("Dedicated component stage cleanup failed at %s", root);
    return -1;
}

int prox_dedicated_prepare(const char *root, const ProxDedicatedExpected *expected,
                           const char *manifest_hash, ProxDedicatedJob *out)
{
    ProxDedicatedJob job = {0};
    ProxDedicatedUpdate parsed;
    int result = -1;
    if (!root || !valid_root(root) || !expected || !out ||
        !manifest_hash || !prox_hash_text(manifest_hash) || !expected->size ||
        expected->size > PROX_DEDICATED_MAX_BYTES || prox_stopping())
        return -1;
    unsigned char *bytes = malloc(expected->size);
    if (!bytes)
        return -1;
    if (read_package(root, bytes, expected->size) ||
        prox_dedicated_update(bytes, expected->size, expected, &parsed) || prox_stopping())
        goto done;

    strcpy(job.install.root, root);
    strcpy(job.install.manifest_sha256, manifest_hash);
    memcpy(job.install.build_id, expected->build_id, sizeof(expected->build_id));
    snprintf(job.install.source_version, sizeof(job.install.source_version), "%u.%u.%u",
             expected->running[0], expected->running[1], expected->running[2]);
    snprintf(job.install.target_version, sizeof(job.install.target_version), "%u.%u.%u",
             expected->target[0], expected->target[1], expected->target[2]);
    job.install.model = expected->model;
    job.package.size = expected->size;
    hash_text(expected->sha256, job.package.sha256);
    if (write_parts(root, bytes, &parsed, &job))
        goto done;
    *out = job;
    result = 0;
done:
    free(bytes);
    return result;
}
