#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "native_update_probe.h"
static unsigned int size,detail;
static int events;
int prox_probe_test_word(unsigned int *p) { return p==&size || p==&detail; }
void prox_native_probe_event(const char *s,int result) { (void)s; (void)result; ++events; }
int main(void)
{
    int (*init)(const void *,const void *)=prox_native_probe_function(0xf6690a9a);
    int (*connect)(unsigned int *,unsigned int *)=prox_native_probe_function(0xfd675e8d);
    int (*read_data)(void *,unsigned int,unsigned int *)=prox_native_probe_function(0xfa9aa797);
    int (*end)(void)=prox_native_probe_function(0xd6a09757);
    unsigned char bytes[16]={1,2,3};
    assert(!prox_native_probe_function(0x12345678));
    assert(connect(&size,&detail)==(int)0x805f0002);
    assert(init(0,(void *)1)==(int)0x805f000b);
    assert(init((void *)1,(void *)2)==0);
    size=detail=0x1234;
    assert(connect(0,&detail)==(int)0x805f000b && detail==0x1234);
    assert(connect(&size,&detail)==(int)0x805f0004 && !size && !detail);
    assert(read_data(bytes,sizeof(bytes),&detail)==(int)0x805f0002 && bytes[0]==1 && bytes[1]==2);
    assert(end()==0 && connect(&size,&detail)==(int)0x805f0002);
    assert(events==4);
    puts("PASS: diagnostic native provider, simulated latest/state/arguments; no network or data writes");
    return 0;
}
