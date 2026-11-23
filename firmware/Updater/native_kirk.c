/* Reuse retained KIRK implementation as local data crypto, never as a PSP
 * system-service replacement. Original notices remain in included sources. */
#include <stdio.h>
int prox_signed_crypto_message(const char *format,...);
#define printf prox_signed_crypto_message
#define sceUtilsSetFuseID prox_signed_set_fuse
#define sceUtilsBufferCopyWithRange prox_signed_kirk_dispatch
#include "../contrib/PrxEncrypter/kirk_engine.c"
