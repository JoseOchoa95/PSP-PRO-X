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
ifneq ($(filter -DPROX_NATIVE_BACKGROUND=1 -DPROX_NATIVE_PROGRESS_PREVIEW=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_flash_preflight.o ../Common/prox_flash_capacity.o ../Common/prox_flash0_capacity.o
endif
ifneq ($(findstring -DPROX_NATIVE_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_error_text.o
ifeq ($(findstring -DPROX_NATIVE_RESIDENT_QUERY=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_query_cache.o
endif
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
CFLAGS += -DPROX_FORMAT_RESIDENT=1 -DMSFORMAT_TRIAL -DMSDIAG_WRITE_TRIAL -DMSDIRS_CARD_TRIAL -DMSMULTI_CARD_TRIAL -DMSAUTO_FORMAT_TRIAL
INCDIR += ../tools/msdiag ../tools/msformat ../tools/msswitch ../tools/mswrite
VPATH += ../tools/msdiag ../tools/msformat ../tools/msswitch
OBJS += memory_stick_format_platform.o memory_stick_format_review.o memory_stick_format_text.o
OBJS += auto_card.o auto_probe.o psp_media.o media_session.o probe.o stock_reader.o capacity.o
OBJS += job.o layout.o gpt.o card_snapshot.o write_probe.o blank_capacity.o blank_probe.o
OBJS += fat32_init.o fat32_dirs.o fat32_ram.o fat32_probe.o sony_mkdir_ram.o ram_volume.o card_mount.o
FORMAT_EXPORTS = native_format_exports.inc
EXTRA_CLEAN += format-ui.o format-ui.bin memory_stick_format_ui_image.h

memory_stick_format_platform.o: memory_stick_format_ui_image.h
memory_stick_format_ui_image.h: memory_stick_format_ui.c memory_stick_format_ui.h make-format-ui-image.py
	$(CC) -std=gnu11 -Os -ffreestanding -fno-pic -mno-abicalls -G0 -fno-jump-tables -Wall -Wextra -Werror -c memory_stick_format_ui.c -o format-ui.o
	python3 make-format-ui-image.py format-ui.o format-ui.bin $@
endif

OBJS += $(IMPORTS)

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports-native-query.exp
exports-native-query.exp: exports.exp native_query_exports.inc $(FORMAT_EXPORTS)
	python3 -c 'from pathlib import Path; p=Path("exports.exp").read_text(); assert p.count("PSP_END_EXPORTS")==1; Path("$@").write_text(p.replace("PSP_END_EXPORTS", "\n".join(Path(path).read_text() for path in "native_query_exports.inc $(FORMAT_EXPORTS)".split())+"\nPSP_END_EXPORTS"))'
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
ifneq ($(findstring -DPROX_NATIVE_RESIDENT_QUERY=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_resident_loader.o resident_sha256.o
INCDIR += ../Updater/vendor/mbedtls/include
CFLAGS += -DMBEDTLS_CONFIG_FILE=\"prox_resident_sha256_config.h\"
endif
ifneq ($(findstring -DPROX_NATIVE_DEDICATED_OTA=1,$(PROX_NATIVE_UPDATE_FLAGS)),)
OBJS += native_dedicated_cleanup.o
endif

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

resident_sha256.o: ../Updater/vendor/mbedtls/library/sha256.c prox_resident_sha256_config.h
	$(CC) $(CFLAGS) -c $< -o $@
