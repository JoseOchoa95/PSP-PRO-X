#!/bin/sh
set -eu
cd /work
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common \
    tests/native/test_wpa2.c firmware/Common/wpa2_patch.c -o /tmp/test-wpa2
/tmp/test-wpa2
cc -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-sign-compare -I firmware/Common -I tests/native \
    tests/native/test_network_config.c firmware/Common/prox_network.c -o /tmp/test-network
/tmp/test-network
