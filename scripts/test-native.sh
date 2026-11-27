#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I firmware/tools/msformat \
    tests/native/test_ms_format_layout.c firmware/tools/msformat/layout.c \
    firmware/tools/msformat/fat32_init.c -o /tmp/test-ms-format-layout
/tmp/test-ms-format-layout
cc -std=c11 -Wall -Wextra -Werror -DMSDIRS_TRIAL -I firmware/tools/msformat \
    tests/native/test_ms_format_job.c firmware/tools/msformat/job.c firmware/tools/msformat/layout.c \
    firmware/tools/msformat/fat32_init.c firmware/tools/msformat/fat32_ram.c \
    firmware/tools/msformat/fat32_dirs.c -o /tmp/test-ms-format-job
/tmp/test-ms-format-job
mkdir -p /tmp/prox-signed-include
python3 scripts/native_signed_templates.py firmware/contrib/PrxEncrypter/main.c /tmp/prox-signed-include/native_signed_templates.h
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common -I firmware/Updater -I /tmp/prox-signed-include \
    -c firmware/Updater/native_signed.c -o /tmp/prox-native-signed.o
cc -O2 -c firmware/Updater/native_kirk.c -o /tmp/prox-native-kirk.o
cc -O2 -c firmware/Updater/native_crypto.c -o /tmp/prox-native-crypto.o
cc -O2 -I firmware/Common -I firmware/Updater tests/native/test_native_signed.c firmware/Updater/native_payload.c \
    /tmp/prox-native-signed.o /tmp/prox-native-kirk.o /tmp/prox-native-crypto.o -lz -o /tmp/test-native-signed
/tmp/test-native-signed
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -DPROX_BACKGROUND_TARGET_TEST \
    -I tests/native -I firmware/Common -I firmware/Updater tests/native/test_native_background_target.c \
    firmware/Updater/native_background.c -o /tmp/test-native-background-target
/tmp/test-native-background-target
cc -std=c11 -Wall -Wextra -Werror -DPROX_BACKGROUND_TARGET_TEST -DPROX_NATIVE_DEDICATED_OTA \
    -I tests/native -I firmware/Common -I firmware/Updater tests/native/test_dedicated_launch.c \
    firmware/Updater/native_background.c firmware/Updater/native_completion_builtin.c \
    -Wl,--wrap=fopen -o /tmp/test-dedicated-launch
/tmp/test-dedicated-launch
cc -O2 tests/native/test_signer_template.c \
    firmware/contrib/PrxEncrypter/kirk_engine.c firmware/contrib/PrxEncrypter/crypto.c -lz -o /tmp/test-signer-template
/tmp/test-signer-template
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_ENGINE_TEST -DPROX_BACKGROUND_ENGINE -Dmain=prox_native_engine_main \
    -I tests/native -I firmware/Common -I firmware/Updater tests/native/test_native_engine.c \
    firmware/tools/nativeinstaller/main.c -o /tmp/test-native-engine
/tmp/test-native-engine
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_ENGINE_TEST -DPROX_BACKGROUND_ENGINE -DPROX_NATIVE_DEDICATED_OTA \
    -Dmain=prox_native_engine_main -I tests/native -I firmware/Common -I firmware/Updater -I firmware/tools/nativeinstaller \
    tests/native/test_native_engine.c firmware/tools/nativeinstaller/main.c -o /tmp/test-dedicated-engine
/tmp/test-dedicated-engine
sh scripts/test-mount-access.sh
cd /work
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common -I firmware/Updater \
    tests/native/test_native_payload.c firmware/Updater/native_payload.c -lz -o /tmp/test-native-payload
/tmp/test-native-payload
cc -std=c99 -Wall -Wextra -Werror -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\" \
    -I firmware/Common -I firmware/Updater -I artifacts/phase3/https-native/mbedtls/include \
    tests/native/test_dedicated_update.c firmware/Updater/dedicated_update.c firmware/Updater/native_payload.c \
    firmware/Updater/policy.c -L artifacts/phase3/https-native/mbedtls/library -lmbedcrypto -lz -o /tmp/test-dedicated-update
/tmp/test-dedicated-update
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_install_state.c firmware/Vshctrl/native_install_state.c -o /tmp/test-native-install-state
/tmp/test-native-install-state
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_DEDICATED_OTA -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_install_state.c firmware/Vshctrl/native_install_state.c -o /tmp/test-dedicated-install-state
/tmp/test-dedicated-install-state
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_FLASH_TEST -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\" \
    -I firmware/Common -I firmware/Updater -I artifacts/phase3/https-native/mbedtls/include \
    tests/native/test_native_flash.c firmware/Updater/native_flash.c \
    -L artifacts/phase3/https-native/mbedtls/library -lmbedcrypto -o /tmp/test-native-flash
/tmp/test-native-flash
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_FLASH_PREFLIGHT_TEST \
    -I tests/native -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_flash_preflight.c firmware/Vshctrl/native_flash_preflight.c -o /tmp/test-native-flash-preflight
/tmp/test-native-flash-preflight
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common tests/native/test_install_preview_lock.c -o /tmp/test-install-preview-lock
/tmp/test-install-preview-lock
cc -std=c99 -Wall -Wextra -Werror -DPROX_INSTALL_PREVIEW_TEST -I tests/native -I firmware/Common tests/native/test_install_preview_module.c -o /tmp/test-install-preview-module
/tmp/test-install-preview-module
cc -std=c99 -Wall -Wextra -Werror -DPROX_INSTALL_LOADER_TEST -I tests/native -I firmware/Common -I firmware/Updater \
    tests/native/test_install_preview_loader.c -o /tmp/test-install-preview-loader
/tmp/test-install-preview-loader
cc -std=c99 -Wall -Wextra -Werror -DPROX_ERROR_TEXT_NATIVE -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_completion_preview.c firmware/Vshctrl/native_preview_stream.c firmware/Vshctrl/native_error_text.c -o /tmp/test-completion-preview
/tmp/test-completion-preview
cc -std=c99 -Wall -Wextra -Werror -I firmware/Updater \
    tests/native/test_completion_rco.c firmware/Updater/native_completion_text.c -o /tmp/test-completion-rco
/tmp/test-completion-rco
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
    -I tests/native -I firmware/Common -I firmware/Vshctrl tests/native/test_native_progress_preview.c \
    firmware/Vshctrl/native_offer_preview.c firmware/Vshctrl/native_preview_stream.c -o /tmp/test-native-progress-preview
/tmp/test-native-progress-preview
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -DPROX_NATIVE_UI_PROBE -DPROX_NATIVE_PROBE_TEST \
    -DPROX_NATIVE_OFFER_PREVIEW -DPROX_NATIVE_PROGRESS_PREVIEW -DPROX_NATIVE_PREVIEW_TEST \
    -I tests/native -I firmware/Common -I firmware/Vshctrl tests/native/test_native_update_probe_dispatch.c \
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
cc -std=c99 -Wall -Wextra -Werror -DPROX_NQ_PROVIDER_TEST -DPROX_NATIVE_OTA -DPROX_NATIVE_PREVIEW_TEST -DPROX_ERROR_TEXT_NATIVE -DPROX_NATIVE_INSTALL_PREVIEW \
    -I tests/native -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_ota_provider.c firmware/Vshctrl/native_query_provider.c \
    firmware/Vshctrl/native_query_state.c firmware/Vshctrl/native_offer_preview.c \
    firmware/Vshctrl/native_preview_stream.c firmware/Vshctrl/native_error_text.c -o /tmp/test-native-completion-provider
/tmp/test-native-completion-provider
cc -std=c99 -Wall -Wextra -Werror -DPROX_NQ_PROVIDER_TEST -DPROX_NATIVE_OTA -DPROX_NATIVE_PREVIEW_TEST -DPROX_ERROR_TEXT_NATIVE -DPROX_NATIVE_BACKGROUND \
    -I tests/native -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_ota_provider.c firmware/Vshctrl/native_query_provider.c \
    firmware/Vshctrl/native_install_state.c firmware/Vshctrl/native_query_state.c firmware/Vshctrl/native_offer_preview.c \
    firmware/Vshctrl/native_preview_stream.c firmware/Vshctrl/native_error_text.c -o /tmp/test-native-background-provider
/tmp/test-native-background-provider
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_SERVICE_TEST -DPROX_BUILD_ID=\"native-service-fixture\" \
    -I tests/native -I firmware/Common -I firmware/Updater -I firmware/Vshctrl \
    tests/native/test_native_service.c firmware/Updater/native_service.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-native-service
/tmp/test-native-service
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_SERVICE_TEST -DPROX_NATIVE_INSTALL_PREVIEW -DPROX_BUILD_ID=\"native-service-fixture\" \
    -I tests/native -I firmware/Common -I firmware/Updater -I firmware/Vshctrl \
    tests/native/test_native_service.c firmware/Updater/native_service.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-native-install-preview-service
/tmp/test-native-install-preview-service
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_SERVICE_TEST -DPROX_NATIVE_BACKGROUND -DPROX_BUILD_ID=\"native-service-fixture\" \
    -I tests/native -I firmware/Common -I firmware/Updater -I firmware/Vshctrl \
    tests/native/test_native_service.c firmware/Updater/native_service.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-native-background-service
/tmp/test-native-background-service
handoff_fixture=$(mktemp -d)
mkdir "$handoff_fixture/Updater"
python3 scripts/dedicated_update.py "$handoff_fixture" pro-x-123456789abc "$handoff_fixture/fixture" --fixture
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_SERVICE_TEST -DPROX_NATIVE_OTA \
    -DPROX_NATIVE_RESIDENT_QUERY -DPROX_NATIVE_DEDICATED_PREVIEW -DPROX_BUILD_ID=\"dedicated-service-fixture\" \
    -I "$handoff_fixture/Updater" -I tests/native -I firmware/Common -I firmware/Updater -I firmware/Vshctrl \
    -I firmware/tools/nativeinstaller tests/native/test_native_service.c firmware/Updater/native_service.c \
    firmware/Updater/policy.c firmware/Vshctrl/native_query_state.c -o /tmp/test-dedicated-handoff-service
/tmp/test-dedicated-handoff-service
cc -std=c11 -Wall -Wextra -Werror -DPROX_NATIVE_SERVICE_TEST -DPROX_NATIVE_OTA -DPROX_NATIVE_BACKGROUND \
    -DPROX_NATIVE_RESIDENT_QUERY -DPROX_NATIVE_DEDICATED_OTA -DPROX_BUILD_ID=\"dedicated-writable-fixture\" \
    -I tests/native -I firmware/Common -I firmware/Updater -I firmware/Vshctrl -I firmware/tools/nativeinstaller \
    tests/native/test_native_service.c firmware/Updater/native_service.c firmware/Updater/policy.c \
    firmware/Vshctrl/native_query_state.c -o /tmp/test-dedicated-writable-service
/tmp/test-dedicated-writable-service
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -DPROX_NATIVE_UI_PROBE -DPROX_NATIVE_PROBE_TEST -DPROX_NATIVE_OTA -DPROX_NATIVE_PREVIEW_TEST \
    -I tests/native -I firmware/Vshctrl tests/native/test_native_update_probe_dispatch.c \
    firmware/Vshctrl/custom_update.c firmware/Vshctrl/native_update_dispatch.c \
    firmware/Vshctrl/native_update_probe.c firmware/Vshctrl/native_offer_preview.c \
    firmware/Vshctrl/native_preview_stream.c -o /tmp/test-native-ota-dispatch
/tmp/test-native-ota-dispatch
cc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -DPROX_UPDATE_NATIVE -DPROX_NATIVE_UI_PROBE -DPROX_NATIVE_PROBE_TEST -DPROX_NATIVE_OTA -DPROX_NATIVE_PREVIEW_TEST -DPROX_NATIVE_INSTALL_PREVIEW \
    -I tests/native -I firmware/Common -I firmware/Vshctrl tests/native/test_native_update_probe_dispatch.c \
    firmware/Vshctrl/custom_update.c firmware/Vshctrl/native_update_dispatch.c \
    firmware/Vshctrl/native_update_probe.c firmware/Vshctrl/native_offer_preview.c \
    firmware/Vshctrl/native_preview_stream.c -o /tmp/test-native-completion-dispatch
/tmp/test-native-completion-dispatch
cc -std=c99 -Wall -Wextra -Werror -DPROX_ERROR_TEXT_NATIVE -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_error_text.c firmware/Vshctrl/native_error_text.c -o /tmp/test-native-error-text
/tmp/test-native-error-text
cc -std=c99 -Wall -Wextra -Werror -DPROX_NATIVE_OTA -I tests/native/loader_sdk -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_loader.c firmware/Vshctrl/native_query_loader.c -o /tmp/test-native-query-loader
/tmp/test-native-query-loader
cc -std=c99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -DPROX_NATIVE_RESIDENT_QUERY -DPROX_NATIVE_DEDICATED_OTA \
    -I tests/native/loader_sdk -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_boot_cleanup.c firmware/Vshctrl/native_query_loader.c -o /tmp/test-native-boot-cleanup
for scenario in none directory card internal; do /tmp/test-native-boot-cleanup "$scenario"; done
cc -std=c99 -Wall -Wextra -Werror -DPROX_CACHE_TEST -I tests/native -I tests/native/loader_sdk -I firmware/Common -I firmware/Vshctrl \
    tests/native/test_native_query_cache.c firmware/Vshctrl/native_query_cache.c -o /tmp/test-native-query-cache
/tmp/test-native-query-cache
mkdir -p /tmp/prox-resident-fixture
python3 -c 'from pathlib import Path; Path("/tmp/prox-resident-fixture/image.prx").write_bytes(b"\x7fELF\x01\x01" + bytes(range(64)))'
python3 scripts/generate-resident-image.py /tmp/prox-resident-fixture/image.prx pro-x-123456789abc \
    /tmp/prox-resident-fixture/prox_resident_image.h /tmp/prox-resident-fixture/archive.z
bin2c /tmp/prox-resident-fixture/archive.z /tmp/prox-resident-fixture/resident_archive.h resident_archive
cc -std=gnu99 -Wall -Wextra -Werror -DPROX_NATIVE_CLEANUP_TEST -DPROX_NATIVE_DEDICATED_OTA -DCONFIG_661=1 \
    -DMBEDTLS_CONFIG_FILE=\"prox_resident_sha256_config.h\" \
    -I tests/native -I firmware/Common -I firmware/Vshctrl -I /tmp/prox-resident-fixture \
    -I artifacts/phase3/https-native/mbedtls/include \
    tests/native/test_native_cleanup.c firmware/Vshctrl/native_dedicated_cleanup.c \
    artifacts/phase3/https-native/mbedtls/library/sha256.c \
    artifacts/phase3/https-native/mbedtls/library/platform_util.c -o /tmp/test-native-cleanup
/tmp/test-native-cleanup
cc -std=gnu99 -Wall -Wextra -Werror -DPROX_RESIDENT_LOADER_TEST \
    -DMBEDTLS_CONFIG_FILE=\"prox_resident_sha256_config.h\" \
    -I tests/native -I firmware/Updater -I firmware/Vshctrl -I /tmp/prox-resident-fixture \
    -I artifacts/phase3/https-native/mbedtls/include \
    tests/native/test_resident_image.c firmware/Installer/resident_image.c firmware/Vshctrl/native_resident_loader.c \
    artifacts/phase3/https-native/mbedtls/library/sha256.c -lz -o /tmp/test-resident-image
/tmp/test-resident-image
cc -std=c99 -Wall -Wextra -Werror -I firmware/Common \
    tests/native/test_flash_capacity.c firmware/Common/prox_flash_capacity.c -o /tmp/test-flash-capacity
/tmp/test-flash-capacity
bin2c /tmp/prox-resident-fixture/image.prx /tmp/prox-resident-fixture/flash_capacity_prx.h flash_capacity_prx
cc -std=c99 -Wall -Wextra -Werror -DPROX_CAPACITY_TEST \
    -I tests/native -I firmware/Installer -I firmware/Common -I firmware/Updater \
    -I /tmp/prox-resident-fixture -I artifacts/phase3/https-native/mbedtls/include \
    tests/native/test_installer_capacity.c firmware/Installer/resident_capacity.c \
    artifacts/phase3/https-native/mbedtls/library/sha256.c \
    artifacts/phase3/https-native/mbedtls/library/platform_util.c -o /tmp/test-installer-capacity
/tmp/test-installer-capacity
cc -std=c99 -Wall -Wextra -Werror -DPROX_RESIDENT_COMPLETION_TEST -DPROX_OTA_NATIVE \
    -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\" \
    -I tests/native -I firmware/Installer -I firmware/Common -I firmware/Updater \
    -I artifacts/phase3/https-native/mbedtls/include \
    tests/native/test_resident_completion.c firmware/Installer/resident_completion.c \
    firmware/Updater/native_completion_store.c firmware/Updater/native_completion_text.c \
    firmware/Updater/stage.c firmware/Updater/contract.c firmware/Updater/json.c firmware/Updater/policy.c \
    -L artifacts/phase3/https-native/mbedtls/library -lmbedcrypto -lz -o /tmp/test-resident-completion
if [ -n "${PROX_PRIVATE_COMPLETION_RCO:-}" ]; then
    /tmp/test-resident-completion "$PROX_PRIVATE_COMPLETION_RCO"
else
    /tmp/test-resident-completion
fi
