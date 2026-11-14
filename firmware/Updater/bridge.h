/* PRO-X private services; not part of the legacy PRO ABI. */
#ifndef PROX_UPDATER_BRIDGE_H
#define PROX_UPDATER_BRIDGE_H
int proxUpdateGetState(int selector);
int proxUpdateRandom(void *output, unsigned int length);
#endif
