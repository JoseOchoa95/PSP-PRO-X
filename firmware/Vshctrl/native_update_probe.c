/* GPL-3.0-or-later. DIAGNOSTIC ONLY: the original PSP UI receives a synthetic
 * no-update result. No HTTP, package, flash, installer or replacement UI calls.
 * Signatures/NIDs were inspected in owner capture 003; no copied Sony code.
 * This is not the production authenticated GitHub provider. */
#include "native_update_probe.h"
#ifdef PROX_NATIVE_PROBE_TEST
#include <stdint.h>
extern int prox_probe_test_word(unsigned int *);
#else
#include <pspkernel.h>
#include <pspsdk.h>
#endif
#define PROBE_CURRENT ((int)0x805f0004)
#define PROBE_STATE ((int)0x805f0002)
#define PROBE_ARGUMENT ((int)0x805f000b)
static int initialized;
static int word_valid(unsigned int *p)
{
#ifdef PROX_NATIVE_PROBE_TEST
    return prox_probe_test_word(p);
#else
    unsigned int address=(unsigned int)p;
    return !(address&3) && address>=0x08800000 && address<=0x0bfffffc;
#endif
}
int proxNativeProbeInitEx(const void *allocate,const void *release)
{
    if(!allocate || !release) return PROBE_ARGUMENT;
    initialized=1;
    prox_native_probe_event("native probe InitEx (no network)",0); return 0;
}
int proxNativeProbeSetting(unsigned int ignored)
{ (void)ignored; return 0; }
int proxNativeProbeEnd(void)
{ initialized=0; prox_native_probe_event("native probe End",0); return 0; }
int proxNativeProbeAbort(void)
{ initialized=0; prox_native_probe_event("native probe Abort",0); return 0; }
int proxNativeProbeConnect(unsigned int *size,unsigned int *detail)
{
    if(!word_valid(size) || !word_valid(detail)) return PROBE_ARGUMENT;
    if(!initialized) return PROBE_STATE;
    *size=0; *detail=0;
    prox_native_probe_event("native probe SIMULATED current",PROBE_CURRENT);
    return PROBE_CURRENT;
}
int proxNativeProbeRead(void *buffer,unsigned int capacity,unsigned int *detail)
{
    (void)buffer; (void)capacity;
    if(!word_valid(detail)) return PROBE_ARGUMENT;
    *detail=0;
    prox_native_probe_event("native probe unexpected read blocked",PROBE_STATE);
    return PROBE_STATE;
}
void *prox_native_probe_function(unsigned int nid)
{
    switch(nid) {
    case 0x34243b86: case 0x36d8f34b: case 0x88ff3935:
    case 0xb5db018d: case 0xc1af1076: return proxNativeProbeSetting;
    case 0x4f49c9c1: return proxNativeProbeAbort;
    case 0xd6a09757: return proxNativeProbeEnd;
    case 0xf6690a9a: return proxNativeProbeInitEx;
    case 0xfa9aa797: return proxNativeProbeRead;
    case 0xfd675e8d: return proxNativeProbeConnect;
    default: return 0;
    }
}
