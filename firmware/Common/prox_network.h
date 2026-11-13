/* PRO-X private network preference. GPL-3.0-or-later. */
#ifndef PROX_NETWORK_H
#define PROX_NETWORK_H
#include <stddef.h>
#define PROX_NETWORK_PATH "flash1:/prox_network.cfg"
#define PROX_NETWORK_SIZE 12
enum ProxNetworkStatus {
    PROX_NETWORK_VALID, PROX_NETWORK_MISSING, PROX_NETWORK_INVALID,
    PROX_NETWORK_IO_ERROR, PROX_NETWORK_RECOVERED
};
int prox_network_decode(const unsigned char *bytes, size_t size, int *enabled);
void prox_network_encode(unsigned char bytes[PROX_NETWORK_SIZE], int enabled);
enum ProxNetworkStatus prox_network_read(int *enabled);
int prox_network_save(int enabled);
#endif
