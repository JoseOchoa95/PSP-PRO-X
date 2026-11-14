/* Deliberately small ZIP profile: stored entries, no extras/comments, no links,
 * exact allowlist, all local/central records checked before any extraction.
 * No ZIP library chooses paths and no flash is accessed here. GPL-3.0-or-later. */
#include "stage.h"
#include "transport.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#ifdef PROX_OTA_FAULT_TEST
extern int prox_native_rename(const char *,const char *);
#define rename prox_native_rename
#endif
#include <mbedtls/sha256.h>
#ifdef PROX_OTA_NATIVE
#include <sys/stat.h>
#include <unistd.h>
static int make_dir(const char *p) { return mkdir(p,0700); }
static int remove_dir(const char *p) { return rmdir(p); }
#else
#include <pspiofilemgr.h>
static int make_dir(const char *p) { return sceIoMkdir(p,0700); }
static int remove_dir(const char *p) { return sceIoRmdir(p); }
#endif

static unsigned int read16(const unsigned char *p) { return p[0] | ((unsigned int)p[1]<<8); }
static unsigned int read32(const unsigned char *p) { return read16(p) | (read16(p+2)<<16); }
static int path(char out[256], const char *root, const char *name)
{ int n=snprintf(out,256,"%s/%s",root,name); return n < 0 || n >= 256 ? -1 : 0; }
static int hash_result(const unsigned char sha[32], const char *expected)
{
    char hex[65]; int i;
    for (i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",sha[i]);
    return strcmp(hex,expected) ? -1 : 0;
}
static int header(FILE *file, unsigned int size)
{
    unsigned char b[40],metadata[0x150]; unsigned int prev=40,offset=0,end=0,sfo_size=0; int i;
    if (size < 44 || fseek(file,0,SEEK_SET) || fread(b,1,40,file) != 40 ||
        memcmp(b,"\0PBP",4) || read32(b+4) != 0x10000 || read32(b+8) != 40) return -1;
    for (i=0;i<8;++i) {
        unsigned int n=read32(b+8+4*i);
        if (n < prev || n > size) return -1;
        prev=n;
        if (i == 1) sfo_size=n-40;
        if (i == 6) offset=n;
        if (i == 7) end=n;
    }
    /* PARAM.SFO header, entry table, key terminators and value bounds. */
    if (sfo_size < 20 || sfo_size > 65536 || fseek(file,40,SEEK_SET) || fread(metadata,1,20,file) != 20 ||
        memcmp(metadata,"\0PSF",4) || read32(metadata+4) != 0x101) return -1;
    {
        unsigned int keys=read32(metadata+8),values=read32(metadata+12),count=read32(metadata+16),entry;
        if (!count || count > 64 || keys < 20+16*count || values < keys || values > sfo_size) return -1;
        for (entry=0;entry<count;++entry) {
            unsigned int key,length,capacity,data,format,k;
            if (fseek(file,40+20+16*entry,SEEK_SET) || fread(metadata,1,16,file) != 16) return -1;
            key=read16(metadata); format=read16(metadata+2); length=read32(metadata+4);
            capacity=read32(metadata+8); data=read32(metadata+12);
            if (key >= values-keys || !length || length > capacity || data > sfo_size-values || capacity > sfo_size-values-data ||
                (format != 0x204 && format != 0x404 && format != 4) ||
                (format == 0x404 && (length != 4 || capacity != 4))) return -1;
            if (fseek(file,40+keys+key,SEEK_SET)) return -1;
            for (k=0;k<64 && k < values-keys-key;++k) { int c=fgetc(file); if (c < 0) return -1; if (!c) break; }
            if (!k || k == 64 || k == values-keys-key) return -1;
            if (format == 0x204) {
                /* Legacy mksfo counts TITLE's characters without its NUL,
                 * while other strings include the NUL. Both must terminate
                 * inside the allocated value, never outside its capacity. */
                if (fseek(file,40+values+data,SEEK_SET)) return -1;
                for (k=0;k<capacity;++k) { int c=fgetc(file); if (c < 0) return -1; if (!c) break; }
                if (k == capacity || (k != length && k+1 != length)) return -1;
            }
        }
    }
    if (end-offset < 52 || fseek(file,(long)offset,SEEK_SET) || fread(metadata,1,52,file) != 52) return -1;
    if (!memcmp(metadata,"~PSP",4)) {
        if (end-offset < 0x150 || read32(metadata+0x2c) != end-offset) return -1;
    } else if (!memcmp(metadata,"\177ELF",4)) {
        unsigned int programs=read16(metadata+44),program_offset=read32(metadata+28);
        if (metadata[4] != 1 || metadata[5] != 1 || metadata[6] != 1 || read16(metadata+18) != 8 ||
            read32(metadata+20) != 1 || read16(metadata+40) != 52 || !programs || programs > 32 ||
            read16(metadata+42) != 32 || program_offset < 52 || program_offset > end-offset ||
            programs*32 > end-offset-program_offset) return -1;
    } else return -1;
    return 0;
}
static int file_check(const char *name, unsigned int size, const char *expected, int pbp,int cancellable)
{
    unsigned char buf[4096],sha[32]; unsigned int total=0;
    mbedtls_sha256_context ctx; FILE *file; size_t n; int rc=-1;
    if (!prox_hash_text(expected) || !size || size > PROX_UPDATE_MAX) return -1;
    file=fopen(name,"rb"); if (!file) return -1;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts(&ctx,0)) goto done;
    while ((n=fread(buf,1,sizeof(buf),file)) != 0) {
        if ((cancellable && prox_stopping()) || n > size-total || mbedtls_sha256_update(&ctx,buf,n)) goto done;
        total+=(unsigned int)n;
    }
    if (ferror(file) || total != size || mbedtls_sha256_finish(&ctx,sha) || hash_result(sha,expected) ||
        (pbp && header(file,size))) goto done;
    rc=0;
done:
    mbedtls_sha256_free(&ctx); if (fclose(file)) rc=-1; return rc;
}
int prox_file_check(const char *name,unsigned int size,const char *expected,int pbp)
{ return file_check(name,size,expected,pbp,1); }
static const char *const directories[]={"PROUPDATE","FastRecovery","PROXUPDATE","licenses","licenses/mbedtls"};
int prox_stage_dirs(const char *root)
{
    unsigned int i; char target[256];
    for (i=0;i<sizeof(directories)/sizeof(directories[0]);++i)
        if (path(target,root,directories[i]) || make_dir(target) < 0) return -1;
    return 0;
}
void prox_stage_remove(const char *root)
{
    unsigned int i; char target[256];
    for (i=0;i<PROX_UPDATE_FILES;++i) if (!path(target,root,prox_update_names[i])) remove(target);
    if (!path(target,root,"pro-x-update.json")) remove(target);
    if (!path(target,root,"update.part")) remove(target);
    /* Launcher-generated files, within this client's newly created stage only. */
    if (!path(target,root,"PROUPDATE/installer.prx")) remove(target);
    if (!path(target,root,"PROUPDATE/Rebootex.prx")) remove(target);
    for (i=(unsigned int)(sizeof(directories)/sizeof(directories[0])); i; --i)
        if (!path(target,root,directories[i-1])) remove_dir(target);
    remove_dir(root);
}
int prox_stage_check(const char *root, const ProxManifest *m)
{
    int i; char target[256];
    for (i=0;i<PROX_UPDATE_FILES;++i)
        if (path(target,root,prox_update_names[i]) || prox_file_check(target,m->files[i].size,m->files[i].sha256,i < 3)) return -1;
    return 0;
}
typedef struct { unsigned int offset, data, size, crc; int id; } Entry;
int prox_zip_stage(const char *zip, const char *root, const ProxManifest *m)
{
    FILE *f=NULL,*out=NULL; unsigned char b[46],buffer[4096]; Entry entries[PROX_UPDATE_FILES];
    unsigned int seen=0,central,cdsize,pos,local=0; int i,k,rc=-1;
    char name[97],target[256];
    /* Recheck the completed download on storage, not only its network digest. */
    if (prox_file_check(zip,m->package_size,m->package_sha256,0)) return -1;
    f=fopen(zip,"rb"); if (!f || m->package_size < 22 || fseek(f,(long)m->package_size-22,SEEK_SET) || fread(b,1,22,f) != 22 ||
        read32(b) != 0x06054b50 || read16(b+4) || read16(b+6) || read16(b+8) != PROX_UPDATE_FILES ||
        read16(b+10) != PROX_UPDATE_FILES || read16(b+20)) goto done;
    cdsize=read32(b+12); central=read32(b+16);
    if (central > m->package_size-22 || cdsize != m->package_size-22-central) goto done;
    pos=central;
    for (i=0;i<PROX_UPDATE_FILES;++i) {
        unsigned int len,mode; int id;
        if (pos > m->package_size-22 || m->package_size-22-pos < 46 || fseek(f,(long)pos,SEEK_SET) || fread(b,1,46,f) != 46 ||
            read32(b) != 0x02014b50 || read16(b+6) > 20 || read16(b+8) || read16(b+10) ||
            read16(b+30) || read16(b+32) || read16(b+34) || read16(b+36)) goto done;
        mode=read32(b+38);
        if ((mode & 0x10) || ((mode>>16)&0xf000) != 0x8000) goto done;
        len=read16(b+28); if (!len || len >= sizeof(name) || len > m->package_size-22-pos-46 || fread(name,1,len,f) != len) goto done;
        name[len]=0; if (strlen(name) != len) goto done;
        for (id=0;id<PROX_UPDATE_FILES;++id) if (!strcmp(name,prox_update_names[id])) break;
        if (id == PROX_UPDATE_FILES || (seen & (1u<<id))) goto done;
        seen |= 1u<<id;
        entries[i].id=id; entries[i].offset=read32(b+42); entries[i].size=read32(b+24); entries[i].crc=read32(b+16);
        if (read32(b+20) != entries[i].size || entries[i].size != m->files[id].size) goto done;
        pos+=46+len;
    }
    if (pos != central+cdsize || seen != (1u<<PROX_UPDATE_FILES)-1) goto done;
    for (i=0;i<PROX_UPDATE_FILES;++i) for (k=i+1;k<PROX_UPDATE_FILES;++k)
        if (entries[k].offset < entries[i].offset) { Entry swap=entries[i]; entries[i]=entries[k]; entries[k]=swap; }
    for (i=0;i<PROX_UPDATE_FILES;++i) {
        Entry *e=&entries[i]; unsigned int len=(unsigned int)strlen(prox_update_names[e->id]);
        if (e->offset != local || local > central || central-local < 30+len || fseek(f,(long)local,SEEK_SET) || fread(b,1,30,f) != 30 ||
            read32(b) != 0x04034b50 || read16(b+4) > 20 || read16(b+6) || read16(b+8) || read32(b+14) != e->crc ||
            read32(b+18) != e->size || read32(b+22) != e->size || read16(b+26) != len || read16(b+28) ||
            fread(name,1,len,f) != len || memcmp(name,prox_update_names[e->id],len)) goto done;
        e->data=local+30+len;
        if (e->size > central-e->data) goto done;
        local=e->data+e->size;
    }
    if (local != central) goto done;
    /* All paths, sizes and record extents are validated before any file write. */
    for (i=0;i<PROX_UPDATE_FILES;++i) {
        Entry *e=&entries[i]; unsigned int remaining=e->size; uLong crc=crc32(0L,Z_NULL,0);
        if (path(target,root,prox_update_names[e->id]) || fseek(f,(long)e->data,SEEK_SET)) goto done;
        out=fopen(target,"wb"); if (!out) goto done;
        while (remaining) {
            size_t n=remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            if (prox_stopping() || fread(buffer,1,n,f) != n || fwrite(buffer,1,n,out) != n) goto done;
            crc=crc32(crc,buffer,(uInt)n); remaining-=(unsigned int)n;
        }
        if (fclose(out)) { out=NULL; goto done; } out=NULL;
        if ((unsigned int)crc != e->crc || prox_file_check(target,e->size,m->files[e->id].sha256,e->id < 3)) goto done;
    }
    rc=0;
done:
    if (out) fclose(out);
    if (f && fclose(f)) rc=-1;
    return rc;
}
static int destination(char out[256], const char *game, int id, const char *suffix)
{
    int n;
    if (id < 4) n=snprintf(out,256,"%s/%s%s",game,prox_update_names[id],suffix);
    else n=snprintf(out,256,"%s/PROXUPDATE/%s%s",game,prox_update_names[id],suffix);
    return n < 0 || n >= 256 ? -1 : 0;
}
int prox_storage_prepare(const char *root, const char *game, const ProxManifest *m)
{
    char source[256],target[256],extra[256]; unsigned char buffer[4096]; int i;
    /* Refuse all stale attempts before creating any new adjacent file. */
    for (i=0;i<PROX_UPDATE_FILES;++i) {
        FILE *exists;
        if (destination(target,game,i,".prox-new") || destination(extra,game,i,".prox-old")) return -1;
        exists=fopen(extra,"rb"); if (exists) { fclose(exists); return -1; }
        exists=fopen(target,"rb"); if (exists) { fclose(exists); return -1; }
    }
    /* Canonical executable directories must already exist from manual install.
     * Legal files stay under PROXUPDATE; create their fixed subdirectories. */
    if (path(extra,game,"PROXUPDATE/licenses")) return -1;
    make_dir(extra);
    if (path(extra,game,"PROXUPDATE/licenses/mbedtls")) return -1;
    make_dir(extra);
    for (i=0;i<PROX_UPDATE_FILES;++i) {
        FILE *in,*out; size_t n; int failed=0;
        if (path(source,root,prox_update_names[i]) || destination(target,game,i,".prox-new") ||
            destination(extra,game,i,".prox-old")) goto failure;
        in=fopen(source,"rb"); if (!in) goto failure;
        out=fopen(target,"wb"); if (!out) { fclose(in); goto failure; }
        while ((n=fread(buffer,1,sizeof(buffer),in)) != 0)
            if (prox_stopping() || fwrite(buffer,1,n,out) != n) { failed=1; break; }
        if (ferror(in)) failed=1;
        if (fclose(in)) failed=1;
        if (fclose(out)) failed=1;
        if (failed || prox_file_check(target,m->files[i].size,m->files[i].sha256,i < 3)) goto failure;
    }
    return 0;
failure:
    /* Only files this invocation could have created after the full preflight. */
    do { if (!destination(target,game,i,".prox-new")) remove(target); } while (i-- > 0);
    return -1;
}
void prox_storage_discard(const char *game,const ProxManifest *m)
{
    int i; char target[256];
    for (i=0;i<PROX_UPDATE_FILES;++i)
        if (!destination(target,game,i,".prox-new") && !file_check(target,m->files[i].size,m->files[i].sha256,i < 3,0)) remove(target);
}
int prox_storage_commit(const char *game, const ProxManifest *m)
{
    char target[256],temp[256],backup[256]; int i,changed=0; unsigned int originals=0;
    for (i=0;i<PROX_UPDATE_FILES;++i) {
        FILE *exists;
        if (destination(temp,game,i,".prox-new") || destination(backup,game,i,".prox-old") ||
            prox_file_check(temp,m->files[i].size,m->files[i].sha256,i < 3)) return -1;
        exists=fopen(backup,"rb"); if (exists) { fclose(exists); return -1; }
    }
    for (i=0;i<PROX_UPDATE_FILES;++i) {
        FILE *old;
        int existed;
        if (destination(target,game,i,"") || destination(temp,game,i,".prox-new") || destination(backup,game,i,".prox-old") ||
            prox_file_check(temp,m->files[i].size,m->files[i].sha256,i < 3)) goto rollback;
        old=fopen(target,"rb"); existed=old != NULL; if (old) fclose(old);
        if (existed && rename(target,backup)) goto rollback;
        if (rename(temp,target)) { if (existed && rename(backup,target)) prox_log("Storage restore failed; keep .prox-old backups"); goto rollback; }
        if (existed) originals |= 1u<<i;
        changed=i+1;
    }
    /* All replacements succeeded; remove only the exact owned backups. */
    for (i=0;i<PROX_UPDATE_FILES;++i) if (!destination(backup,game,i,".prox-old")) remove(backup);
    return 0;
rollback:
    while (changed--) {
        destination(target,game,changed,""); destination(temp,game,changed,".prox-new"); destination(backup,game,changed,".prox-old");
        if (rename(target,temp) || ((originals & (1u<<changed)) && rename(backup,target)))
            prox_log("Storage rollback incomplete; preserve stage/.prox-old for recovery");
    }
    return -1;
}
