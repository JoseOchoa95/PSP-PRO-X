#define _GNU_SOURCE
/* Actual host files and real SHA-256; no PSP device/flash is available. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <mbedtls/sha256.h>
#include "native_flash.h"
#ifdef PROX_DEDICATED_GENERATED
#include "dedicated_job.h"
#include "dedicated_writer.h"
#include "expected_update.h"
#include "prox_native_flash_paths.h"
#include "stage.h"
#include "pspkernel.h"
#include <fcntl.h>
#endif
static int mode,phase,writes,destructive,failures;
static FILE *output;
static char destination[256],logs[65536];static size_t used;
#ifdef PROX_DEDICATED_GENERATED
static const char *trial_worker;
static const ProxNativePart *trial_part;
#endif
void prox_log(const char *format,...)
{int n;va_list args;va_start(args,format);n=vsnprintf(logs+used,sizeof(logs)-used,format,args);va_end(args);assert(n>=0 && (size_t)n<sizeof(logs)-used);used+=(size_t)n;}
/* A regression to ANY rename would fail even in the success fixture. */
int rename(const char *a,const char *b)
{(void)a;(void)b;assert(!"6.61 flashfat cannot rename");return -1;}
FILE *prox_flash_test_open(const char *p,const char *flags)
{
#ifdef PROX_DEDICATED_GENERATED
    if (trial_worker && phase == 1 && !strcmp(flags, "wb") && !strstr(p, ".prox-")) {
        if (!destructive && mode != 14)
            assert(!strcmp(p, trial_worker));
        if (!strcmp(p, prox_native_flash_path(1)))
            assert(!prox_file_check(trial_worker, trial_part->size, trial_part->sha256, 0));
    }
#endif
    FILE *f=fopen(p,flags);
    if(!strcmp(flags,"wb")) {
        output=f;strcpy(destination,p);
        if(!strstr(p,".prox-")) {
            ++destructive;
            if(phase==1 && mode==6 && !failures){++failures;assert(f && !fclose(f));errno=ENOMEM;return NULL;}
        }
    }
    return f;
}
size_t prox_flash_test_write(const void *p,size_t size,size_t n,FILE *f)
{
    int inject=0;++writes;assert(f==output);
    if(mode==1 && !failures)inject=1;
    if(mode==5 && strstr(destination,".prox-old") && !failures)inject=1;
    if((mode==2 || mode==7) && phase==1 && destructive==2 && !failures)inject=1;
    if(mode==7 && phase==2 && !strstr(destination,".prox-") && failures==1)inject=1;
#ifdef PROX_DEDICATED_GENERATED
    if (mode == 12 && !strcmp(destination, prox_native_flash_path(1)) &&
        ((phase == 1 && !failures) || (phase == 2 && failures == 1)))
        inject = 1;
    if (mode == 16 && phase == 1 && !failures && !strcmp(destination, trial_worker))
        inject = 1;
#endif
    if(inject){size_t actual;++failures;assert(n>1);actual=fwrite(p,size,n-1,f);errno=ENOSPC;return actual;}
    return fwrite(p,size,n,f);
}
static void hash_bytes(const unsigned char *data,size_t n,char out[65])
{unsigned char digest[32];unsigned int i;assert(!mbedtls_sha256(data,n,digest,0));for(i=0;i<32;++i)snprintf(out+2*i,3,"%02x",digest[i]);}
int prox_file_check(const char *p,unsigned int size,const char *hash,int pbp)
{
    FILE *file = fopen(p, "rb");
    unsigned char buffer[4096], digest[32];
    char actual[65];
    mbedtls_sha256_context context;
    size_t count, total = 0;
    (void)pbp;
    if (!file)
        return -1;
    mbedtls_sha256_init(&context);
    assert(!mbedtls_sha256_starts(&context, 0));
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        total += count;
        assert(!mbedtls_sha256_update(&context, buffer, count));
    }
    assert(!ferror(file) && !fclose(file));
    assert(!mbedtls_sha256_finish(&context, digest));
    mbedtls_sha256_free(&context);
    for (unsigned int i = 0; i < 32; ++i)
        snprintf(actual + 2 * i, 3, "%02x", digest[i]);
    return total == size && !strcmp(actual, hash) ? 0 : -1;
}
static void dirs(const char *path)
{char copy[256];size_t i;strcpy(copy,path);for(i=0;copy[i];++i)if(copy[i]=='/'){copy[i]=0;if(i)assert(!mkdir(copy,0700)||errno==EEXIST);copy[i]='/';}}
static void fill(const char *path,int c,unsigned int n)
{FILE *f;dirs(path);f=fopen(path,"wb");assert(f);while(n--)assert(fputc(c,f)!=EOF);assert(!fclose(f));}
static void old_file(const char *path)
{FILE *f=fopen(path,"rb");int c,n=0;assert(f);while((c=fgetc(f))!=EOF){assert(c==127);++n;}assert(n==32 && !fclose(f));}
#ifdef PROX_DEDICATED_GENERATED
int sceIoMkdir(const char *path, int mode) { return mkdir(path, mode); }
int sceIoRmdir(const char *path) { return rmdir(path); }
int sceIoRemove(const char *path) { return unlink(path); }
int sceIoOpen(const char *path, int flags, int mode)
{
    assert(flags == (PSP_O_CREAT | PSP_O_WRONLY | PSP_O_EXCL));
    return open(path, O_CREAT | O_WRONLY | O_EXCL, mode);
}
int sceIoWrite(int fd, const void *bytes, unsigned int count) { return (int)write(fd, bytes, count); }
int sceIoClose(int fd) { return close(fd); }
int sceIoSync(const char *device, unsigned int flags) { assert(!strcmp(device, "ms0:") && !flags); return 0; }
int prox_stopping(void) { return 0; }

static void write_part(const char *path, const unsigned char *bytes, unsigned int size)
{
    dirs(path);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, size, file) == size && !fclose(file));
}

static void verify_generated_transaction(const char *path)
{
    static const char previous_build[] = "pro-x-123456789abc";
    char previous[PROX_RESIDENT_SLOT_BYTES], worker_path[PROX_RESIDENT_SLOT_BYTES];
    char staged[256], witness[256], worker_temporary[256];
    unsigned char *data = malloc(fixture.size);
    ProxDedicatedUpdate update;
    FILE *file = fopen(path, "rb");
    assert(data && file && fread(data, 1, fixture.size, file) == fixture.size);
    assert(fgetc(file) == EOF && !ferror(file) && !fclose(file));
    assert(!prox_dedicated_update(data, fixture.size, &fixture, &update));
    const char unterminated[19] = "pro-x-123456789abcd";
    assert(prox_resident_slot("pro-x-a", previous) < 0);
    assert(prox_resident_slot(unterminated, previous) < 0);
    assert(!prox_resident_slot(previous_build, previous));
    assert(!prox_resident_slot(fixture.build_id, worker_path) && strcmp(previous, worker_path));
    assert(snprintf(witness, sizeof(witness), "%s.prox-old", prox_native_flash_path(1)) > 0);
    assert(snprintf(worker_temporary, sizeof(worker_temporary), "%s.prox-new", worker_path) > 0);

    for (mode = 11; mode <= 18; ++mode) {
        const char directory[] = "ms0:/PROXDIAG";
        const char root[] = "ms0:/PROXDIAG/.dedicated-00000001-00000002";
        const char marker[] = "ms0:/PROXDIAG/.dedicated-committed";
        ProxDedicatedJob prepared;
        ProxNativeFlash state = {0};
        char run[16], eboot[256];
        sprintf(run, "run-%d", mode);
        assert(!mkdir(run, 0700) && !chdir(run));
        writes = destructive = failures = phase = 0;
        used = 0;
        const char snapshot_hash[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        assert(snprintf(eboot, sizeof(eboot), "%s/EBOOT.PBP", root) > 0);
        write_part(eboot, data, fixture.size);
        assert(!prox_dedicated_prepare(root, &fixture, snapshot_hash, &prepared));
        const ProxNativeInstallJob job = prepared.install;
        const ProxNativePart worker = prepared.worker;
        assert(!strcmp(job.root, root) && !strcmp(job.build_id, fixture.build_id));
        assert(!strcmp(job.manifest_sha256, snapshot_hash) && job.model == fixture.model);
        assert(!strcmp(job.source_version, "0.0.0") && prepared.package.size == fixture.size);
        assert(!prox_dedicated_verify(&prepared));
        fill(previous, 127, 32);
        for (unsigned int i = 0; i < PROX_NATIVE_FLASH_FILES; ++i)
            fill(prox_native_flash_path(i), 127, 32);
        assert(prox_dedicated_record_commit(&prepared, &state) < 0 && access(marker, F_OK));
        if (mode == 13)
            fill(worker_path, 127, 32);
        if (mode == 14) {
            const ProxDedicatedPart *part = &update.parts[PROX_DEDICATED_WORKER];
            write_part(worker_path, data + part->offset, part->size);
        }
        if (mode == 15) {
            assert(snprintf(staged, sizeof(staged), "%s/native/%02u.prx", root, PROX_DEDICATED_WORKER) > 0);
            fill(staged, 127, 32);
        }
        if (mode == 17) {
            assert(snprintf(staged, sizeof(staged), "%s/native/10.prx", root) > 0);
            fill(staged, 127, 32);
        }
        if (mode == 18)
            fill(eboot, 127, 32);

        if (mode == 13 || mode == 15 || mode >= 17) {
            assert(prox_dedicated_writer_prepare(&prepared, 1, 2, &state) < 0 && !writes && !destructive);
            for (unsigned int i = 0; i < PROX_NATIVE_FLASH_FILES; ++i)
                old_file(prox_native_flash_path(i));
            if (mode == 13)
                old_file(worker_path);
        } else {
            assert(prox_dedicated_writer_prepare(&prepared, 1, 3, &state) < 0 && !writes && !destructive);
            assert(!prox_dedicated_writer_prepare(&prepared, 1, 2, &state) && !destructive);
            old_file(witness);
            trial_worker = worker_path;
            trial_part = &worker;
            phase = 1;
            if (mode == 12 || mode == 16) {
                assert(prox_native_flash_commit(&job, &state) < 0);
                phase = 2;
                if (mode == 12) {
                    assert(prox_native_flash_rollback(&job, &state) < 0);
                    assert(!prox_file_check(worker_path, worker.size, worker.sha256, 0));
                    assert(!prox_file_check(worker_temporary, worker.size, worker.sha256, 0));
                    old_file(witness);
                }
                phase = 3;
                assert(!prox_native_flash_rollback(&job, &state));
                for (unsigned int i = 0; i < PROX_NATIVE_FLASH_FILES; ++i)
                    old_file(prox_native_flash_path(i));
                assert(access(worker_path, F_OK) && access(worker_temporary, F_OK));
            } else {
                assert(!prox_native_flash_commit(&job, &state));
                assert(!prox_file_check(worker_path, worker.size, worker.sha256, 0));
                assert(!prox_dedicated_record_commit(&prepared, &state));
                assert(prox_dedicated_record_commit(&prepared, &state) < 0 && !access(marker, F_OK));
                assert(prox_native_flash_finalize(&job, &state) < 0);
                assert(prox_dedicated_writer_finalize(&prepared, &state, previous_build) < 0);
                assert(prox_dedicated_cleanup_committed(directory, previous_build, job.target_version, 1) < 0);
                assert(prox_dedicated_cleanup_committed(directory, fixture.build_id, job.source_version, 1) < 0);
                assert(prox_dedicated_cleanup_committed(directory, fixture.build_id, job.target_version, 0) < 0);
                assert(prox_dedicated_cleanup_committed(directory, fixture.build_id, job.target_version, 1) < 0);
                old_file(witness);
                assert(!access(eboot, F_OK));
                ProxNativeFlash cleanup_start;
                ProxDedicatedJob restored;
                assert(!prox_dedicated_read_committed(directory, fixture.build_id, job.target_version, 1, &restored, &cleanup_start));
                assert(!strcmp(restored.install.build_id, job.build_id) && cleanup_start.prepared == state.prepared);
                assert(cleanup_start.backups == state.backups && !strcmp(cleanup_start.before[1].sha256, state.before[1].sha256));
                state=cleanup_start;
                if (mode == 11) {
                    FILE *record = fopen(marker, "r+b");
                    assert(record && !fseek(record, 8, SEEK_SET));
                    int original = fgetc(record);
                    assert(original != EOF && !fseek(record, 8, SEEK_SET));
                    assert(fputc(original ^ 1, record) != EOF && !fflush(record));
                    assert(prox_dedicated_cleanup_committed(directory, fixture.build_id, job.target_version, 1) < 0);
                    assert(!access(eboot, F_OK) && !fseek(record, 8, SEEK_SET));
                    assert(fputc(original, record) != EOF && !fclose(record));
                    fill(witness, 99, 32);
                    assert(prox_dedicated_writer_finalize(&prepared, &state, fixture.build_id) < 0);
                    assert(!access(witness, F_OK) && !access(eboot, F_OK));
                    fill(witness, 127, 32);
                    state = cleanup_start;
                }
                assert(!prox_dedicated_writer_finalize(&prepared, &state, fixture.build_id));
                state = cleanup_start;
                assert(!prox_dedicated_writer_finalize(&prepared, &state, fixture.build_id));
                assert(access(witness, F_OK) && access(worker_temporary, F_OK));
                assert(prox_native_flash_rollback(&job, &state) < 0);
                assert(!prox_file_check(worker_path, worker.size, worker.sha256, 0));
                if (mode == 11) {
                    assert(snprintf(staged, sizeof(staged), "%s/native/keep.txt", root) > 0);
                    fill(staged, 127, 32);
                    assert(prox_dedicated_cleanup_committed(directory, fixture.build_id, job.target_version, 1) < 0);
                    assert(!access(eboot, F_OK) && !access(marker, F_OK));
                    old_file(staged);
                    assert(!unlink(staged));
                }
                if (mode == 14) {
                    fill(eboot, 127, 32);
                    assert(prox_dedicated_cleanup_committed(directory, fixture.build_id, job.target_version, 1) < 0);
                    assert(!access(marker, F_OK));
                    old_file(eboot);
                    write_part(eboot, data, fixture.size);
                }
                assert(!prox_dedicated_cleanup_committed(directory, fixture.build_id, job.target_version, 1));
                assert(access(eboot, F_OK) && access(marker, F_OK) && access(root, F_OK));
            }
            trial_worker = NULL;
        }
        old_file(previous);
        if (mode != 11 && mode != 14) {
            assert(!access(eboot, F_OK));
            assert(!prox_dedicated_remove_parts(&prepared));
        }
        assert(!chdir(".."));
    }
    free(data);
    puts("PASS: compiled dedicated EBOOT checked again at writer entry, rollback and boot-gated finalization, retryable exact card cleanup; isolated host files");
}
#endif

int main(int argc, char **argv)
{
    char temp[]="/tmp/prox-native-flash-XXXXXX",cwd[512],p[256];unsigned int i;ProxNativeInstallJob job={0};
    static const char *const expected[]={"flash0:/kd/_systemctrl.prx","flash0:/kd/_vshctrl.prx","flash0:/kd/_galaxy.prx",
      "flash0:/kd/_stargate.prx","flash0:/kd/_march33.prx","flash0:/kd/_inferno.prx","flash0:/kd/_usbdevice.prx",
      "flash0:/kd/_popcorn.prx","flash0:/vsh/module/_satelite.prx","flash0:/vsh/module/_recovery.prx"};
    assert(getcwd(cwd,sizeof(cwd)) && mkdtemp(temp) && !chdir(temp));
    strcpy(job.root,"ms0:/PSP/GAME/PROXUPDATE/.stage-pro-x-123456789abc-00112233");
    for(mode=0;mode<11;++mode) {
        ProxNativeFlash state={0};char run[16];sprintf(run,"run-%d",mode);assert(!mkdir(run,0700) && !chdir(run));
        writes=destructive=failures=phase=0;used=0;logs[0]=0;
        for(i=0;i<10;++i) {
            unsigned char b[32];assert(!strcmp(prox_native_flash_path(i),expected[i]));
            job.parts[i].size=32;memset(b,(int)i+1,sizeof(b));hash_bytes(b,sizeof(b),job.parts[i].sha256);
            snprintf(p,sizeof(p),"%s/native/%02u.prx",job.root,i);fill(p,(int)i+1,32);
            dirs(expected[i]);if(mode!=9)fill(expected[i],mode==3?(int)i+1:127,32);
        }
        if(mode==4){snprintf(p,sizeof(p),"%s.prox-old",expected[0]);fill(p,99,32);}
        if(mode==1 || mode==4 || mode==5) {
            assert(prox_native_flash_prepare(&job,&state)<0 && !state.changed && !destructive);
            for(i=0;i<10;++i)old_file(expected[i]);
            if(mode==4)assert(!writes && strstr(logs,"new=0 old=1"));
            else assert(strstr(logs,"errno=28") && strstr(logs,"FLASH_ROLLBACK result=0"));
        } else {
            { int result=prox_native_flash_prepare(&job,&state);
              if(result || destructive)fprintf(stderr,"mode=%d result=%d destructive=%d\n%s\n",mode,result,destructive,logs);
              assert(!result && !destructive); }
            if(mode!=9 && mode!=3)for(i=0;i<10;++i){old_file(expected[i]);snprintf(p,sizeof(p),"%s.prox-old",expected[i]);old_file(p);}
            if(mode==8){snprintf(p,sizeof(p),"%s.prox-old",expected[9]);fill(p,99,32);}
            if(mode==10){snprintf(p,sizeof(p),"%s.prox-new",expected[9]);fill(p,99,32);}
            phase=1;
            if(mode==2 || mode==6 || mode==7 || mode==8 || mode==10) {
                assert(prox_native_flash_commit(&job,&state)<0);
                if(mode==8 || mode==10)assert(!destructive);
                phase=2;
                if(mode==7) {
                    assert(prox_native_flash_rollback(&job,&state)<0 && state.changed && state.originals && state.backups);
                    snprintf(p,sizeof(p),"%s.prox-old",expected[1]);old_file(p);
                    phase=3;assert(!prox_native_flash_rollback(&job,&state));
                } else assert(!prox_native_flash_rollback(&job,&state));
                for(i=0;i<10;++i)old_file(expected[i]);
            } else {
                assert(!prox_native_flash_commit(&job,&state));
                for(i=0;i<10;++i)assert(!prox_file_check(expected[i],32,job.parts[i].sha256,0));
                assert(!prox_native_flash_finalize(&job,&state) && !state.originals && !state.backups && !state.prepared);
                if(mode==3)assert(!writes && !state.changed);
            }
        }
        assert(!chdir(".."));
    }
#ifdef PROX_DEDICATED_GENERATED
    assert(argc == 2);
    verify_generated_transaction(argv[1]);
#else
    (void)argc;
    (void)argv;
#endif
    assert(!chdir(cwd));
    puts("PASS: actual SHA-256 backups/copies with rename prohibited; all backups before original opens; target/backup corruption, ENOSPC, post-truncate open failure, absent originals, unchanged smart-skip, rollback failure retains exact backup and retry restores; no PSP flash");
}
