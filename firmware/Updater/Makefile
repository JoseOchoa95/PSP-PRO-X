TARGET = prox_updater
OBJS = main.o transport.o policy.o bridge.o json.o contract.o stage.o update.o platform.o storage_power.o resolver.o network_close.o connect_retry.o network_session.o network_profile.o catalog.o native_download.o native_service.o native_query_bridge.o native_trust.o native_ca.o
INCDIR = ../Common ../include vendor/mbedtls/include .
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DPROX_BUILD_ID=\"$(BUILD_ID)\" -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\"
CFLAGS += $(PROX_NATIVE_UPDATE_FLAGS)
ifneq ($(findstring -DPROX_NATIVE_RESIDENT_QUERY=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS := $(filter-out main.o stage.o update.o platform.o storage_power.o native_download.o,$(OBJS)) resident_main.o
endif
ifneq ($(filter -DPROX_NATIVE_DEDICATED_PREVIEW=1 -DPROX_NATIVE_DEDICATED_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_dedicated_stage.o dedicated_update.o dedicated_job.o dedicated_writer.o native_payload.o stage.o storage_power.o
INCDIR += ../tools/nativeinstaller
vpath dedicated_writer.c ../tools/nativeinstaller
endif
ifneq ($(findstring -DPROX_NATIVE_INSTALL_PREVIEW=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_install_preview.o native_completion_text.o
LIBS_EXTRA = -lpspkubridge
endif
ifneq ($(findstring -DPROX_NATIVE_DEDICATED_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_background.o native_completion_text.o native_completion_store.o
LIBS_EXTRA = -lpspkubridge
else ifneq ($(findstring -DPROX_NATIVE_BACKGROUND=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_background.o native_payload.o native_completion_text.o native_completion_store.o native_signed.o native_kirk.o native_crypto.o
LIBS_EXTRA = -lpspkubridge
endif
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBDIR = vendor/mbedtls/library ../libs
LIBS = -lmbedtls -lmbedx509 -lmbedcrypto -lz -lpspsystemctrl_user -lpspwlan -lpsppower $(LIBS_EXTRA)
EXTRA_TARGETS = EBOOT.PBP
include ../Common/prox_product.mk
PSP_EBOOT_TITLE = PRO-X v$(PROX_PRODUCT_VERSION) System Update
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
# Preserve the SDK default goal and compile legacy crypto with its old flags.
# Project-owned decoder and adapters retain -Wall -Wextra -Werror.
native_kirk.o: native_kirk.c
	$(CC) $(filter-out -Wall -Wextra -Werror,$(CFLAGS)) -c $< -o $@
native_crypto.o: native_crypto.c
	$(CC) $(filter-out -Wall -Wextra -Werror,$(CFLAGS)) -c $< -o $@
