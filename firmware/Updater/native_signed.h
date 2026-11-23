/* Decode authenticated PRO signer payloads as data. GPL-3.0-or-later. */
#ifndef PROX_NATIVE_SIGNED_H
#define PROX_NATIVE_SIGNED_H
#include "native_payload.h"
/* Caller must authenticate/pin the complete PBP before supplying DATA.PSP.
 * No module loading, code execution, permission changes or firmware writes. */
int prox_native_signed_scan(ProxNativePayloadScan *,const unsigned char *,size_t);
#endif
