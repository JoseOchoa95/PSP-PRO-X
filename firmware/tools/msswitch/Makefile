TARGET = msswitch
OBJS = main.o partition_view.o fat32_probe.o native_fat_read.o fat32_init.o fat32_ram.o probe.o stock_reader.o systemctrl_exit.o
VPATH = ../msdiag ../msformat
INCDIR = ../../include ../msdiag ../mswrite ../msformat
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DMSSWITCH_BUILD_ID=\"$(BUILD_ID)\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBS = -lpspctrl_driver
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X Partition Switch Trial
ifeq ($(RAM_DIRECTORY_TRIAL),1)
OBJS = dirs_main.o sony_mkdir_ram.o fat32_dirs.o fat32_init.o fat32_ram.o systemctrl_exit.o
CFLAGS += -DMSDIRS_TRIAL
PSP_EBOOT_TITLE = PRO-X FAT32 RAM Directory Trial
endif
ifeq ($(PHYSICAL_DIRECTORY_TRIAL),1)
OBJS = dirs_main.o dirs_card.o sony_mkdir_ram.o fat32_dirs.o fat32_init.o fat32_ram.o fat32_probe.o partition_view.o probe.o stock_reader.o systemctrl_exit.o
CFLAGS += -DMSDIRS_TRIAL -DMSDIRS_CARD_TRIAL -DMSFORMAT_TRIAL -DMSDIAG_WRITE_TRIAL
PSP_EBOOT_TITLE = PRO-X FAT32 Physical Directory Trial
endif
ifeq ($(PHYSICAL_FORMAT_TRIAL),1)
CFLAGS += -DMSFORMAT_TRIAL -DMSDIAG_WRITE_TRIAL
PSP_EBOOT_TITLE = PRO-X FAT32 Partition Format Trial
endif
ifeq ($(WHOLE_CARD_TRIAL),1)
OBJS += capacity.o layout.o job.o card_mount.o
CFLAGS += -DMSMULTI_CARD_TRIAL
PSP_EBOOT_TITLE = PRO-X Memory Stick Whole Card Trial
endif
ifeq ($(AUTO_FORMAT_TRIAL),1)
OBJS := $(filter-out dirs_card.o,$(OBJS)) auto_card.o psp_media.o auto_probe.o card_snapshot.o write_probe.o gpt.o
CFLAGS += -DMSAUTO_FORMAT_TRIAL
PSP_EBOOT_TITLE = PRO-X Memory Stick Auto Format
endif
ifeq ($(WRITE_PATH_TRIAL),1)
OBJS := $(filter-out card_mount.o job.o,$(OBJS))
CFLAGS += -DMSWRITE_PATH_TRIAL
PSP_EBOOT_TITLE = PRO-X Memory Stick Write Path Trial
endif
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

systemctrl_exit.o: ../../SystemControl/libs/libpspsystemctrl_kernel/SystemCtrlForKernel.S
	$(CC) $(CFLAGS) -DF_SystemCtrlForKernel_0000 -DF_SystemCtrlForKernel_0001 -c $< -o $@
