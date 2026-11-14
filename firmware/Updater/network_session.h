/* User-mode stack lifecycle; no profile mutation. GPL-3.0-or-later. */
#ifndef PROX_NETWORK_SESSION_H
#define PROX_NETWORK_SESSION_H
int prox_network_begin(void);
int prox_network_end(void);
int prox_network_connect(int profile);
int prox_network_retry(unsigned int request_start);
/* Read-only sceUtility 0x4FED24D8, absent from the pinned SDK's stubs.
 * PSP NID database: pspdev.github.io/psplibdoc/modules/utility.prx_sceUtility.html */
int proxUtilityGetLatestProfile(int *profile);
#endif
