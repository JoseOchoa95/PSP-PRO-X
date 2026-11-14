TARGET = prox_updater
OBJS = main.o transport.o policy.o bridge.o json.o contract.o stage.o update.o platform.o storage_power.o resolver.o network_close.o connect_retry.o network_session.o network_profile.o
INCDIR = ../Common ../include vendor/mbedtls/include .
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DPROX_BUILD_ID=\"$(BUILD_ID)\" -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBDIR = vendor/mbedtls/library ../libs
LIBS = -lmbedtls -lmbedx509 -lmbedcrypto -lz -lpspsystemctrl_user -lpspwlan -lpsppower
EXTRA_TARGETS = EBOOT.PBP
include ../Common/prox_product.mk
PSP_EBOOT_TITLE = PRO-X v$(PROX_PRODUCT_VERSION) System Update
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
