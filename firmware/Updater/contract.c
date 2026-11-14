/* No release name/body heuristic: flags, numeric versions and exact assets.
 * Only the versioned allowlisted install files are accepted. GPL-3.0-or-later. */
#include "contract.h"
#include "policy.h"
#include <stdio.h>
#include <string.h>

const char *const prox_update_names[PROX_UPDATE_FILES] = {
    "PROUPDATE/EBOOT.PBP", "FastRecovery/EBOOT.PBP", "PROXUPDATE/EBOOT.PBP",
    "PROXUPDATE/cacert.pem", "LICENSE", "THIRD_PARTY_NOTICES.md", "SOURCE.md",
    "licenses/mbedtls/LICENSE"
};
int prox_hash_text(const char *text)
{
    int i;
    if (strlen(text) != 64) return 0;
    for (i=0;i<64;++i) if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return 0;
    return 1;
}
static int text(const Pj *j, int t, const char *key, char *out, size_t cap)
{ return pj_text(j,pj_field(j,t,key),out,cap); }
static int number(const Pj *j, int t, const char *key, unsigned int *out)
{ return pj_uint(j,pj_field(j,t,key),out); }
static int equal(const Pj *j, int t, const char *key, const char *expected)
{ return pj_equal(j,pj_field(j,t,key),expected); }
static int boolean(const Pj *j, int t, const char *key)
{ int i=pj_field(j,t,key); return i < 0 ? -1 : j->tokens[i].type == PJ_FALSE ? 0 : j->tokens[i].type == PJ_TRUE ? 1 : -1; }

static int asset(const Pj *j, int t, const char *tag, const char *name, ProxAsset *out)
{
    char expected[256], digest[80]; int d;
    memset(out,0,sizeof(*out));
    if (number(j,t,"size",&out->size) || !out->size || out->size > PROX_UPDATE_MAX ||
        text(j,t,"browser_download_url",out->url,sizeof(out->url))) return -1;
    if (snprintf(expected,sizeof(expected),"https://github.com/JoseOchoa95/PSP-PRO-X/releases/download/%s/%s",tag,name) >= (int)sizeof(expected) ||
        strcmp(expected,out->url)) return -1;
    d=pj_field(j,t,"digest");
    if (d >= 0 && j->tokens[d].type != PJ_NULL) {
        if (pj_text(j,d,digest,sizeof(digest)) || strncmp(digest,"sha256:",7) || !prox_hash_text(digest+7)) return -1;
        strcpy(out->digest,digest+7);
    }
    return 0;
}
int prox_release(const Pj *j, int t, const unsigned int running[3], ProxRelease *out)
{
    int a,i,have_manifest=0,have_package=0;
    char name[96], wanted[96];
    memset(out,0,sizeof(*out));
    if (t < 0 || t >= j->used || j->tokens[t].type != PJ_OBJECT || boolean(j,t,"draft") < 0 || boolean(j,t,"prerelease") < 0) return -1;
    if (boolean(j,t,"draft") || boolean(j,t,"prerelease")) return 0;
    if (text(j,t,"tag_name",out->tag,sizeof(out->tag)) || !prox_stable(0,0,out->tag)) return 0;
    strcpy(out->version,out->tag+1);
    if (prox_version(out->version,out->numbers) || prox_compare(out->numbers,running) <= 0) return 0;
    snprintf(wanted,sizeof(wanted),"pro-x-%s-661-update.zip",out->version);
    a=pj_field(j,t,"assets");
    if (a < 0 || j->tokens[a].type != PJ_ARRAY || j->tokens[a].count > 100) return -1;
    for (i=a+1;i<j->tokens[a].next;i=j->tokens[i].next) {
        if (text(j,i,"name",name,sizeof(name))) continue;
        if (!strcmp(name,"pro-x-update.json")) {
            if (have_manifest++ || asset(j,i,out->tag,name,&out->manifest) || out->manifest.size > 32768) return -1;
        } else if (!strcmp(name,wanted)) {
            if (have_package++ || asset(j,i,out->tag,name,&out->package)) return -1;
        }
    }
    return have_manifest && have_package ? 1 : 0;
}
int prox_manifest(const Pj *j, const ProxRelease *release, const unsigned int running[3],
                  const unsigned int client[3], unsigned int model, ProxManifest *out)
{
    unsigned int n,minimum[3],seen=0,total=0;
    char buffer[96];
    int a,i;
    memset(out,0,sizeof(*out));
    if (number(j,0,"schema_version",&n) || n != 1 || !equal(j,0,"product","PRO-X") ||
        !equal(j,0,"repository","JoseOchoa95/PSP-PRO-X") || !equal(j,0,"channel","stable") ||
        !equal(j,0,"firmware_target","6.61") || !equal(j,0,"tag",release->tag) ||
        text(j,0,"version",out->version,sizeof(out->version)) || strcmp(out->version,release->version) ||
        prox_version(out->version,out->numbers) || prox_compare(out->numbers,running) <= 0 ||
        text(j,0,"minimum_updater_version",buffer,sizeof(buffer)) || prox_version(buffer,minimum) ||
        prox_compare(minimum,client) > 0 || text(j,0,"build_id",out->build_id,sizeof(out->build_id)) ||
        text(j,0,"summary",out->summary,sizeof(out->summary))) return -1;
    if (strlen(out->build_id) != 18 || strncmp(out->build_id,"pro-x-",6)) return -1;
    for (i=6;i<18;++i) if (!((out->build_id[i]>='0' && out->build_id[i]<='9') || (out->build_id[i]>='a' && out->build_id[i]<='f'))) return -1;
    a=pj_field(j,0,"supported_models");
    if (a < 0 || j->tokens[a].type != PJ_ARRAY || !j->tokens[a].count || j->tokens[a].count > 16 || model >= 32) return -1;
    for (i=a+1;i<j->tokens[a].next;i=j->tokens[i].next) {
        if (pj_uint(j,i,&n) || n >= 16 || (out->models & (1u<<n))) return -1;
        out->models |= 1u<<n;
    }
    if (!(out->models & (1u<<model))) return -1;
    a=pj_field(j,0,"package_asset");
    snprintf(buffer,sizeof(buffer),"pro-x-%s-661-update.zip",out->version);
    if (!equal(j,a,"name",buffer) || number(j,a,"size",&out->package_size) ||
        out->package_size != release->package.size || !out->package_size || out->package_size > PROX_UPDATE_MAX ||
        text(j,a,"sha256",out->package_sha256,sizeof(out->package_sha256)) || !prox_hash_text(out->package_sha256) ||
        (release->package.digest[0] && strcmp(release->package.digest,out->package_sha256))) return -1;
    strcpy(out->package_name,buffer);
    a=pj_field(j,0,"files");
    if (a < 0 || j->tokens[a].type != PJ_ARRAY || j->tokens[a].count != PROX_UPDATE_FILES) return -1;
    for (i=a+1;i<j->tokens[a].next;i=j->tokens[i].next) {
        int id;
        if (text(j,i,"path",buffer,sizeof(buffer))) return -1;
        for (id=0;id<PROX_UPDATE_FILES;++id) if (!strcmp(buffer,prox_update_names[id])) break;
        if (id == PROX_UPDATE_FILES || (seen & (1u<<id))) return -1;
        seen |= 1u<<id;
        if (number(j,i,"size",&out->files[id].size) || !out->files[id].size ||
            out->files[id].size > 4u*1024u*1024u ||
            text(j,i,"sha256",out->files[id].sha256,sizeof(out->files[id].sha256)) || !prox_hash_text(out->files[id].sha256)) return -1;
        total += out->files[id].size;
    }
    return total < PROX_UPDATE_MAX && seen == (1u<<PROX_UPDATE_FILES)-1 ? 0 : -1;
}
