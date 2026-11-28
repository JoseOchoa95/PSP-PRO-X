TARGET = installer
OBJS = main.o ../Updater/ota_installer.o ../Updater/json.o ../Updater/contract.o \
       ../Updater/policy.o ../Updater/stage.o ../Updater/storage_power.o
EXTRA_CLEAN= galaxy.h march33.h march33_620.h popcorn.h satelite.h stargate.h systemctrl.h usbdevice.h vshctrl.h

BUILD_PRX=1

all: $(OBJS)

$(OBJS): pre-build

pre-build:
	./create_module_include.py
	touch main.c

INCDIR = ../include ../Common ../Updater ../Updater/vendor/mbedtls/include
CFLAGS = -Os -G0 -Wall -DMBEDTLS_CONFIG_FILE=\"prox_mbedtls_config.h\"
ifneq ($(findstring -DPROX_NATIVE_RESIDENT_QUERY=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
CFLAGS += -DPROX_NATIVE_RESIDENT_QUERY=1
OBJS += resident_image.o resident_capacity.o installer_flash_capacity.o
installer_flash_capacity.o: ../Common/prox_flash_capacity.c ../Common/prox_flash_capacity.h
	$(CC) $(CFLAGS) -I../Common -c $< -o $@
ifneq ($(findstring -DPROX_NATIVE_DEDICATED_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
CFLAGS += -DPROX_RESIDENT_BUILTIN_COMPLETION=1
else
OBJS += resident_completion.o ../Updater/native_completion_store.o ../Updater/native_completion_text.o
endif
EXTRA_CLEAN += flash_capacity_prx.h
pre-build: flash_capacity_prx.h
flash_capacity_prx.h: flash_capacity/main.c flash_capacity/Makefile flash_capacity/exports.exp flash_capacity_result.h ../Common/prox_flash_capacity.c ../Common/prox_flash0_capacity.c ../Common/prox_flash_capacity.h
	$(MAKE) -C flash_capacity
	bin2c flash_capacity/flash_capacity.prx $@ flash_capacity_prx
resident_capacity.o: flash_capacity_prx.h
endif
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

ifeq ($(CONFIG_620), 1)
CFLAGS += -DCONFIG_620=1
PSP_FW_VERSION = 620
endif

ifeq ($(CONFIG_635), 1)
CFLAGS += -DCONFIG_635=1
PSP_FW_VERSION = 635
endif

ifeq ($(CONFIG_639), 1)
CFLAGS += -DCONFIG_639=1
PSP_FW_VERSION = 639
endif

ifeq ($(CONFIG_660), 1)
CFLAGS += -DCONFIG_660=1
PSP_FW_VERSION = 660
endif

ifeq ($(CONFIG_661), 1)
CFLAGS += -DCONFIG_661=1
PSP_FW_VERSION = 661
endif

LIBDIR = ../libs ../Updater/vendor/mbedtls/library
LDFLAGS =
LIBS = -lmbedcrypto -lz -lpspkubridge -lpsppower

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
