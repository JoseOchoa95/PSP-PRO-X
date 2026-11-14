#include <assert.h>
#include <stdio.h>
#include "policy.h"

int main(void)
{
    unsigned int a[3], b[3];
    char host[80];
    const char *path;
    assert(!prox_version("0.10.0", a)); assert(!prox_version("0.9.0", b));
    assert(prox_compare(a, b) > 0); assert(prox_compare(b, a) < 0);
    assert(prox_compare(a, a) == 0);
    assert(prox_version("v0.3.0-beta", a)); assert(prox_version("v0.3", a));
    assert(prox_version("v0.03.0", a)); assert(prox_version("v2147483648.0.0", a));
    assert(prox_version("v0.3.0\n", a));
    assert(!prox_stable(1, 0, "v0.4.0")); assert(!prox_stable(0, 1, "v0.4.0"));
    assert(!prox_stable(-1, 0, "v0.4.0")); assert(!prox_stable(0, 0, "v0.4.0-rc.1"));
    assert(prox_stable(0, 0, "v0.4.0"));
    assert(!prox_url("https://api.github.com/repos/JoseOchoa95/PSP-PRO-X/releases/latest", host, sizeof(host), &path));
    assert(!prox_url("https://release-assets.githubusercontent.com/github-production-release-asset/1?sig=abc", host, sizeof(host), &path));
    assert(prox_url("http://github.com/", host, sizeof(host), &path));
    assert(prox_url("https://github.com.evil.example/", host, sizeof(host), &path));
    assert(prox_url("https://user@github.com/", host, sizeof(host), &path));
    assert(prox_url("https://github.com:80/", host, sizeof(host), &path));
    assert(prox_url("https://evil.example/", host, sizeof(host), &path));
    assert(prox_url("https://api.github.com/repos/someone/other/releases/latest", host, sizeof(host), &path));
    assert(prox_url("https://github.com/other/repo/releases/download/v1/a.zip", host, sizeof(host), &path));
    assert(prox_url("https://github.com/a\r\nInjected:yes", host, sizeof(host), &path));
    puts("PASS: product SemVer, stable flags, HTTPS/host/redirect destination policy");
    return 0;
}
