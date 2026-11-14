#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "update_bridge_shim.h"

unsigned int psp_fw_version, psp_model;
static int generic_calls, random_calls, fail_on_call, current_k1 = 0x10000000;
int sceKernelDevkitVersion(void) { ++generic_calls; return (int)0x8002013a; }
int sceKernelGetModel(void) { ++generic_calls; return (int)0x8002013a; }
int pspSdkSetK1(int value) { int old = current_k1; current_k1 = value; return old; }
int sceUtilsBufferCopyWithRange(void *output, unsigned int length, void *input, int size, int command)
{
    assert(current_k1 == 0 && length == 20 && input == NULL && size == 0 && command == 14);
    ++random_calls;
    if (random_calls == fail_on_call) return -1;
    memset(output, 0x5a, length);
    return 0;
}
int proxUpdateGetState(int);
int proxUpdateRandom(void *, unsigned int);

int main(void)
{
    unsigned char *memory;
    int i;
    psp_fw_version = 0x06060110; psp_model = 1;
    assert(proxUpdateGetState(0) == 1);
    assert(proxUpdateGetState(4) == (int)psp_fw_version);
    assert(proxUpdateGetState(5) == (int)psp_model);
    /* Read live initialized state, never hardcode one model/firmware value. */
    psp_fw_version = 0x06060010; psp_model = 4;
    assert(proxUpdateGetState(4) == (int)psp_fw_version);
    assert(proxUpdateGetState(5) == 4);
    assert(proxUpdateGetState(-1) < 0 && proxUpdateGetState(99) < 0);
    assert(generic_calls == 0);
    memory = mmap((void *)0x08800000, 4096, PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0);
    assert(memory == (void *)0x08800000);
    assert(!proxUpdateRandom(memory, 40));
    assert(random_calls == 2 && current_k1 == 0x10000000);
    for (i = 0; i < 40; ++i) assert(memory[i] == 0x5a);
    random_calls = 0; fail_on_call = 2;
    assert(proxUpdateRandom(memory, 40) < 0);
    for (i = 0; i < 40; ++i) assert(memory[i] == 0);
    assert(current_k1 == 0x10000000);
    random_calls = 0;
    assert(proxUpdateRandom(NULL, 20) < 0);
    assert(proxUpdateRandom(memory, 0) < 0);
    assert(proxUpdateRandom(memory, 257) < 0);
    assert(proxUpdateRandom((void *)0x88000000, 20) < 0);
    assert(random_calls == 0);
    munmap(memory, 4096);
    puts("PASS: live CrossFW state with poisoned SDK queries, bounded RNG, hardware failure zeroing and K1 restoration");
    return 0;
}
