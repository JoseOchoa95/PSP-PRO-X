/* PRO-X HTTPS endpoint and product-version policy; GPL-3.0-or-later. */
#include "policy.h"
#include <string.h>

int prox_url(const char *url, char *host, size_t capacity, const char **path)
{
    const char *slash, *p;
    size_t length;
    if (strncmp(url, "https://", 8)) return -1;
    slash = strchr(url + 8, '/');
    if (!slash) return -1;
    if (strstr(slash, "/../") || strstr(slash, "/./")) return -1;
    length = (size_t)(slash - url - 8);
    if (!length || length >= capacity) return -1;
    memcpy(host, url + 8, length); host[length] = 0;
    if (strcmp(host, "api.github.com") && strcmp(host, "github.com") &&
        strcmp(host, "release-assets.githubusercontent.com") &&
        strcmp(host, "objects.githubusercontent.com")) return -1;
    if (!strcmp(host, "api.github.com")) {
        const char *prefix = "/repos/JoseOchoa95/PSP-PRO-X";
        size_t n = strlen(prefix);
        if (strncmp(slash, prefix, n) || (slash[n] && slash[n] != '/' && slash[n] != '?')) return -1;
    }
    if (!strcmp(host, "github.com")) {
        const char *prefix = "/JoseOchoa95/PSP-PRO-X/releases/download/";
        if (strncmp(slash, prefix, strlen(prefix))) return -1;
    }
    for (p = slash; *p; ++p)
        if ((unsigned char)*p < 33 || (unsigned char)*p > 126 || *p == '#' || *p == '\\') return -1;
    if ((size_t)(p - url) >= 2048) return -1;
    *path = slash;
    return 0;
}

int prox_version(const char *text, unsigned int version[3])
{
    int component;
    if (*text == 'v') ++text;
    for (component = 0; component < 3; ++component) {
        unsigned int number = 0;
        if (*text < '0' || *text > '9') return -1;
        if (*text == '0' && text[1] >= '0' && text[1] <= '9') return -1;
        while (*text >= '0' && *text <= '9') {
            unsigned int digit = (unsigned int)(*text++ - '0');
            if (number > (2147483647u - digit) / 10) return -1;
            number = number * 10 + digit;
        }
        version[component] = number;
        if (component < 2) { if (*text++ != '.') return -1; }
    }
    return *text ? -1 : 0;
}

int prox_compare(const unsigned int a[3], const unsigned int b[3])
{
    int i;
    for (i = 0; i < 3; ++i) if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}

int prox_stable(int draft, int prerelease, const char *tag)
{
    unsigned int version[3];
    return draft == 0 && prerelease == 0 && tag[0] == 'v' && !prox_version(tag, version);
}
