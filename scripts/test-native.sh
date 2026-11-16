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
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_PROBE_TEST -I firmware/Vshctrl \
    tests/native/test_native_update_probe.c firmware/Vshctrl/native_update_probe.c -o /tmp/test-native-update-probe
/tmp/test-native-update-probe
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -DPROX_NATIVE_UI_PROBE -DPROX_NATIVE_PROBE_TEST -I tests/native -I firmware/Vshctrl \
    tests/native/test_native_update_probe_dispatch.c firmware/Vshctrl/custom_update.c \
    firmware/Vshctrl/native_update_dispatch.c firmware/Vshctrl/native_update_probe.c -o /tmp/test-native-update-probe-dispatch
/tmp/test-native-update-probe-dispatch
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_state.c firmware/Vshctrl/native_query_state.c -o /tmp/test-native-query-state
/tmp/test-native-query-state
cc -std=c99 -Wall -Wextra -Werror -DPROX_NQ_PROVIDER_TEST \
    -I tests/native -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_provider.c firmware/Vshctrl/native_query_provider.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-native-query-provider
/tmp/test-native-query-provider
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common tests/native/test_pbp_payload.c -o /tmp/test-pbp-payload
/tmp/test-pbp-payload
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_PREVIEW_TEST \
    -I tests/native -I firmware/Vshctrl tests/native/test_native_offer_preview.c \
    firmware/Vshctrl/native_offer_preview.c -o /tmp/test-native-offer-guard
/tmp/test-native-offer-guard
cc -std=c99 -Wall -Wextra -Werror -DPROX_NQ_PROVIDER_TEST -DPROX_NATIVE_OFFER_PREVIEW \
    -I tests/native -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_provider.c firmware/Vshctrl/native_query_provider.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-native-preview-provider
/tmp/test-native-preview-provider
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -DPROX_NATIVE_UI_PROBE -DPROX_NATIVE_PROBE_TEST \
    -DPROX_NATIVE_OFFER_PREVIEW -DPROX_NATIVE_PREVIEW_TEST -I tests/native -I firmware/Vshctrl \
    tests/native/test_native_update_probe_dispatch.c firmware/Vshctrl/custom_update.c \
    firmware/Vshctrl/native_update_dispatch.c firmware/Vshctrl/native_update_probe.c \
    firmware/Vshctrl/native_offer_preview.c -o /tmp/test-native-preview-dispatch
/tmp/test-native-preview-dispatch
cc -std=c99 -Wall -Wextra -Werror -I firmware/Vshctrl \
    tests/native/test_native_preview_stream.c firmware/Vshctrl/native_preview_stream.c -o /tmp/test-native-preview-stream
/tmp/test-native-preview-stream
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_PREVIEW_TEST -DPROX_NATIVE_PROGRESS_PREVIEW \
    -I tests/native -I firmware/Vshctrl tests/native/test_native_progress_preview.c \
    firmware/Vshctrl/native_offer_preview.c firmware/Vshctrl/native_preview_stream.c -o /tmp/test-native-progress-preview
/tmp/test-native-progress-preview
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -DPROX_NATIVE_UI_PROBE -DPROX_NATIVE_PROBE_TEST \
    -DPROX_NATIVE_OFFER_PREVIEW -DPROX_NATIVE_PROGRESS_PREVIEW -DPROX_NATIVE_PREVIEW_TEST \
    -I tests/native -I firmware/Vshctrl tests/native/test_native_update_probe_dispatch.c \
    firmware/Vshctrl/custom_update.c firmware/Vshctrl/native_update_dispatch.c \
    firmware/Vshctrl/native_update_probe.c firmware/Vshctrl/native_offer_preview.c \
    firmware/Vshctrl/native_preview_stream.c -o /tmp/test-native-progress-dispatch
/tmp/test-native-progress-dispatch
cc -std=c99 -Wall -Wextra -Werror -DPROX_NQ_PROVIDER_TEST \
    -DPROX_NATIVE_OFFER_PREVIEW -DPROX_NATIVE_PROGRESS_PREVIEW \
    -I tests/native -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_provider.c firmware/Vshctrl/native_query_provider.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-native-progress-provider
/tmp/test-native-progress-provider
cc -std=c99 -Wall -Wextra -Werror -DPROX_NQ_PROVIDER_TEST -DPROX_NATIVE_PREVIEW_TEST -DPROX_NATIVE_OTA -DPROX_ERROR_TEXT_NATIVE \
    -I tests/native -I firmware/Common -I firmware/Vshctrl tests/native/test_native_ota_provider.c \
    firmware/Vshctrl/native_query_provider.c firmware/Vshctrl/native_query_state.c \
    firmware/Vshctrl/native_offer_preview.c firmware/Vshctrl/native_preview_stream.c \
    firmware/Vshctrl/native_error_text.c -o /tmp/test-native-ota-provider
/tmp/test-native-ota-provider
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_SERVICE_TEST -DPROX_BUILD_ID=\"native-service-fixture\" \
    -I tests/native -I firmware/Common -I firmware/Updater -I firmware/Vshctrl \
    tests/native/test_native_service.c firmware/Updater/native_service.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-native-service
/tmp/test-native-service
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -DPROX_NATIVE_UI_PROBE -DPROX_NATIVE_PROBE_TEST -DPROX_NATIVE_OTA -DPROX_NATIVE_PREVIEW_TEST \
    -I tests/native -I firmware/Vshctrl tests/native/test_native_update_probe_dispatch.c \
    firmware/Vshctrl/custom_update.c firmware/Vshctrl/native_update_dispatch.c \
    firmware/Vshctrl/native_update_probe.c firmware/Vshctrl/native_offer_preview.c \
    firmware/Vshctrl/native_preview_stream.c -o /tmp/test-native-ota-dispatch
/tmp/test-native-ota-dispatch
cc -std=c99 -Wall -Wextra -Werror -DPROX_ERROR_TEXT_NATIVE -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_error_text.c firmware/Vshctrl/native_error_text.c -o /tmp/test-native-error-text
/tmp/test-native-error-text
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_OTA -I tests/native/loader_sdk -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_loader.c firmware/Vshctrl/native_query_loader.c -o /tmp/test-native-query-loader
/tmp/test-native-query-loader
cc -std=c99 -Wall -Wextra -Werror -DPROX_CACHE_TEST -I tests/native -I tests/native/loader_sdk -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_cache.c firmware/Vshctrl/native_query_cache.c -o /tmp/test-native-query-cache
/tmp/test-native-query-cache
