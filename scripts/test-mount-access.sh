#!/bin/sh
set -eu
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common -I firmware/Vshctrl tests/native/test_native_mount_access.c firmware/Vshctrl/native_mount_access.c -o /tmp/prox-mount-access
/tmp/prox-mount-access
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common -I firmware/Vshctrl tests/native/test_native_mount_lease.c firmware/Vshctrl/native_mount_lease.c -o /tmp/prox-mount-lease
/tmp/prox-mount-lease
cc -std=c99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections -I firmware/Common -I firmware/Vshctrl -I tests/native tests/native/test_native_mount_rpc.c firmware/Vshctrl/native_install_state.c -o /tmp/prox-mount-rpc
/tmp/prox-mount-rpc
cc -std=c99 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -I firmware/Common -I firmware/Vshctrl -I tests/native tests/native/test_native_mount_platform.c firmware/Vshctrl/native_mount_access.c firmware/Vshctrl/native_mount_lease.c -o /tmp/prox-mount-platform
/tmp/prox-mount-platform
