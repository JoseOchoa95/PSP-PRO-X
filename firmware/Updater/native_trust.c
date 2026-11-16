/* Resident trust uses exactly the build's pinned public bundle. */
#include "transport.h"
extern const unsigned char prox_native_ca[],prox_native_ca_end[];
const char prox_native_cardless_marker[]="PROX-NATIVE-CARDLESS-V1";
int prox_native_trust(void)
{ return prox_transport_set_trust(prox_native_ca,(size_t)(prox_native_ca_end-prox_native_ca)); }
