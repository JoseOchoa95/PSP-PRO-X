#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "prox_product.h"
#include "version_display.h"

static unsigned short *memory;
static int missing, fail_alloc, bad_head, allocations, releases, k1 = 0x10000000;
int pspSdkSetK1(int value) { int old = k1; k1 = value; return old; }
static int allocate(int partition, const char *name, int type, unsigned int size, void *address)
{
    assert(k1 == 0 && partition == 2 && type == 1 && address == NULL);
    assert(!strcmp(name,"PRO-X system version"));
    assert(size == sizeof("6.61 PRO-X " PROX_PRODUCT_VERSION)*2);
    ++allocations; return fail_alloc ? -1 : 7;
}
static void *head(int id) { assert(id == 7); return bad_head ? (void *)0x88000000 : memory; }
static int release(int id) { assert(id == 7); ++releases; return 0; }
uintptr_t sctrlHENFindFunction(char *module, char *library, unsigned int nid)
{
    assert(!strcmp(module,"sceSystemMemoryManager") && !strcmp(library,"SysMemForKernel"));
    if (missing) return 0;
    switch(nid) {
    case 0x237DBD4F: return (uintptr_t)allocate;
    case 0x9D9A5BA1: return (uintptr_t)head;
    case 0xB6D61D02: return (uintptr_t)release;
    default: assert(0); return 0;
    }
}
int main(void)
{
    /* Synthetic module with the pinned upstream anchor and an intentionally
     * non-LUI target pair. The old test's assumed original pair was too narrow.
     * These target words are test data, not claimed Sony instructions. */
    unsigned int code[24] = {0};
    unsigned int before[24];
    const char text[] = "6.61 PRO-X " PROX_PRODUCT_VERSION;
    size_t i;
    memory = mmap((void *)0x08802000,4096,PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(memory == (void *)0x08802000);
    code[0]=0xdeadbeef; code[1]=0x34c600c9; code[3]=0;
    code[6]=0x01234567; code[7]=0xabcdef01; code[8]=0xcafebabe;
    memset(memory,0xa5,4096); memcpy(before,code,sizeof(code));
    assert(prox_system_version_patch(code,sizeof(code),3)<0);
    assert(prox_system_version_patch(code,sizeof(code),sizeof(code)-4)<0);
    assert(prox_system_version_patch(code,sizeof(code),SIZE_MAX)<0);
    assert(!memcmp(code,before,sizeof(code)) && allocations==0);
    assert(prox_system_version_patch(code,sizeof(code),28)<0);
    missing=1; assert(prox_system_version_patch(code,sizeof(code),24)<0); missing=0;
    fail_alloc=1; assert(prox_system_version_patch(code,sizeof(code),24)<0); fail_alloc=0;
    bad_head=1; assert(prox_system_version_patch(code,sizeof(code),24)<0); bad_head=0;
    assert(releases==1 && !memcmp(code,before,sizeof(code)) && k1==0x10000000);
    assert(!prox_system_version_patch(code,sizeof(code),24));
    for(i=0;i<sizeof(text);++i) assert(memory[i] == (unsigned char)text[i]);
    assert(memory[sizeof(text)]==0xa5a5); /* no write beyond allocated label */
    assert(code[0]==before[0] && code[8]==before[8]);
    assert((((uintptr_t)(code[6]&0xffff))<<16 | (code[7]&0xffff))==(uintptr_t)memory);
    i=allocations;
    assert(!prox_system_version_patch(code,sizeof(code),24) && allocations==(int)i);
    code[1]=0xdead0000; memcpy(before,code,sizeof(code));
    assert(prox_system_version_patch(code,sizeof(code),24)<0 && !memcmp(code,before,sizeof(code)));
    code[1]=0x34c600c9; code[10]=0x34c600c9; code[12]=0;
    memcpy(before,code,sizeof(code));
    assert(prox_system_version_patch(code,sizeof(code),24)<0 && !memcmp(code,before,sizeof(code)));
    assert(k1==0x10000000);
    munmap(memory,4096);
    puts("PASS: pinned unique anchor/offset, non-LUI original pair, kernel services, UTF-16 bounds, failures and reuse");
    return 0;
}
