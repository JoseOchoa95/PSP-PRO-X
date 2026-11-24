/* Build the pinned Mbed TLS hash without its user-mode runtime dependencies. */
#ifndef PROX_RESIDENT_SHA256_CONFIG_H
#define PROX_RESIDENT_SHA256_CONFIG_H
#define MBEDTLS_SHA256_C
#define MBEDTLS_PLATFORM_ZEROIZE_ALT
#endif
