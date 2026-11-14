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
cc -std=c99 -Wall -Wextra -Werror -I firmware/tools/netdiag \
    tests/native/test_netdiag_protocol.c firmware/tools/netdiag/protocol.c -o /tmp/test-netdiag
/tmp/test-netdiag
cc -std=c99 -Wall -Wextra -Werror -I firmware/Updater \
    tests/native/test_update_policy.c firmware/Updater/policy.c -o /tmp/test-update-policy
/tmp/test-update-policy
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -I tests/native tests/native/test_update_dispatch.c \
    firmware/Vshctrl/custom_update.c -o /tmp/test-update-dispatch
/tmp/test-update-dispatch
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast \
    -DPROX_BRIDGE_NATIVE -I tests/native -I firmware/Common \
    tests/native/test_update_bridge.c firmware/SystemControl/prox_update.c -o /tmp/test-update-bridge
/tmp/test-update-bridge
cc -std=c99 -Wall -Wextra -Werror -DPROX_VERSION_NATIVE \
    -I tests/native -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_version_display.c firmware/Vshctrl/version_display.c -o /tmp/test-version-display
/tmp/test-version-display
cc -std=c99 -Wall -Wextra -Werror -I tests/native -I firmware/Common -I firmware/Updater \
    tests/native/test_updater_ui.c -o /tmp/test-updater-ui
/tmp/test-updater-ui
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common tests/native/test_ota_args.c -o /tmp/test-ota-args
/tmp/test-ota-args
cc -std=c99 -Wall -Wextra -Werror -I tests/native -I firmware/Updater tests/native/test_storage_power.c -o /tmp/test-storage-power
/tmp/test-storage-power
cc -std=c99 -Wall -Wextra -Werror -I tests/native -I firmware/Updater tests/native/test_resolver_supervisor.c -o /tmp/test-resolver-supervisor
/tmp/test-resolver-supervisor
cc -std=c99 -Wall -Wextra -Werror -I tests/native -I firmware/Updater tests/native/test_network_close.c -o /tmp/test-network-close
/tmp/test-network-close
cc -std=c99 -Wall -Wextra -Werror -I firmware/Updater tests/native/test_connect_retry.c firmware/Updater/connect_retry.c -o /tmp/test-connect-retry
/tmp/test-connect-retry
cc -std=c99 -Wall -Wextra -Werror -I tests/native -I firmware/Updater tests/native/test_network_session.c -o /tmp/test-network-session
/tmp/test-network-session
