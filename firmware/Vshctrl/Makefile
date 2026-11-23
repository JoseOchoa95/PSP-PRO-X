all:
	../contrib/pspgz.py vshctrl.prx ../contrib/SystemControl.hdr vshctrl.prx VshCtrl 0x1007

TARGET = vshctrl
OBJS = main.o vshpatch.o custom_update.o version_display.o xmbiso.o isoreader.o virtual_pbp.o vshmenu.o usbdevice.o dirent_track.o vshctrl_patch_offset.o disctype.o hibernation_delete.o ../Common/utils.o ../Common/libs.o ../Common/strsafe.o
OBJS += native_query_provider.o native_query_state.o native_query_loader.o native_completion_preview.o
ifneq ($(findstring -DPROX_NATIVE_BACKGROUND=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_install_state.o
OBJS += native_mount_access.o native_mount_lease.o native_mount_platform.o
CFLAGS += -DPROX_NATIVE_MOUNT_ACCESS=1
endif
ifneq ($(findstring -DPROX_NATIVE_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_error_text.o native_query_cache.o
endif
IMPORTS =

INCDIR = ../Common/ ../include/
CFLAGS += -Os -G0 -Wall -fno-pic
CFLAGS += $(PROX_NATIVE_UPDATE_FLAGS)
ifneq ($(findstring -DPROX_NATIVE_UI_PROBE=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_update_probe.o native_update_dispatch.o
endif
ifneq ($(findstring -DPROX_NATIVE_UI_QUERY=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_update_dispatch.o
endif
ifneq ($(filter -DPROX_NATIVE_OFFER_PREVIEW=1 -DPROX_NATIVE_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_offer_preview.o
endif
ifneq ($(filter -DPROX_NATIVE_PROGRESS_PREVIEW=1 -DPROX_NATIVE_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_preview_stream.o
endif

ifeq ($(DEBUG), 1)
CFLAGS += -DDEBUG=1
OBJS += ../Common/printk.o
endif

ifeq ($(NIGHTLY), 1)
CFLAGS += -DNIGHTLY=\"$(shell hg summary | grep parent | cut -d " " -f 2)\"
endif

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

OBJS += $(IMPORTS)

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports-native-query.exp
exports-native-query.exp: exports.exp native_query_exports.inc
	python3 -c 'from pathlib import Path; p=Path("exports.exp").read_text(); assert p.count("PSP_END_EXPORTS")==1; Path("exports-native-query.exp").write_text(p.replace("PSP_END_EXPORTS",Path("native_query_exports.inc").read_text()+"\nPSP_END_EXPORTS"))'
ifneq ($(filter -DPROX_NATIVE_OFFER_PREVIEW=1 -DPROX_NATIVE_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
PRX_EXPORTS = exports-native-preview.exp
exports-native-preview.exp: exports-native-query.exp native_preview_exports.inc
	python3 -c 'from pathlib import Path; p=Path("exports-native-query.exp").read_text(); assert p.count("PSP_END_EXPORTS")==1; Path("exports-native-preview.exp").write_text(p.replace("PSP_END_EXPORTS",Path("native_preview_exports.inc").read_text()+"\nPSP_END_EXPORTS"))'
endif
ifneq ($(findstring -DPROX_NATIVE_UI_PROBE=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
PRX_EXPORTS = exports-native-probe.exp
exports-native-probe.exp: exports-native-query.exp native_probe_exports.inc
	python3 -c 'from pathlib import Path; p=Path("exports-native-query.exp").read_text(); assert p.count("PSP_END_EXPORTS")==1; Path("exports-native-probe.exp").write_text(p.replace("PSP_END_EXPORTS",Path("native_probe_exports.inc").read_text()+"\nPSP_END_EXPORTS"))'
endif

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

LIBDIR = ../libs
LDFLAGS = -mno-crt0 -nostartfiles
LIBS = -lpspsystemctrl_kernel -lpspusb -lpspusbdevice_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
