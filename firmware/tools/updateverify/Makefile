TARGET = prox_update_verify
OBJS = main.o dedicated_update.o native_payload.o policy.o
vpath %.c ../../Updater
INCDIR = ../../Common ../../Updater ../../include ../../Updater/vendor/mbedtls/include
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBDIR = ../../Updater/vendor/mbedtls/library ../../libs
LIBS = -lmbedcrypto -lz -lpspkubridge
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X Update File Check
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

main.o: expected_update.h
