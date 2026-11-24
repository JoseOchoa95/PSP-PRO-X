TARGET = native_installer
OBJS = main.o bridge.o ota_installer.o json.o contract.o policy.o stage.o storage_power.o native_flash.o native_payload.o dedicated_writer.o dedicated_job.o dedicated_update.o
vpath %.c ../../Updater
INCDIR = ../../Common ../../Updater ../../include ../../Updater/vendor/mbedtls/include
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DPROX_BACKGROUND_ENGINE=1 -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\"
CFLAGS += $(filter -DPROX_NATIVE_DEDICATED_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS))
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBDIR = ../../Updater/vendor/mbedtls/library ../../libs
LIBS = -lmbedcrypto -lz -lpspkubridge -lpsppower
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
