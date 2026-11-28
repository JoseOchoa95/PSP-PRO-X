TARGET = msdiag
OBJS = main.o probe.o capacity.o stock_reader.o systemctrl_exit.o
INCDIR = ../../include
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DMSDIAG_BUILD_ID=\"$(BUILD_ID)\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBS = -lpspctrl_driver
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X Memory Stick Diagnostic
ifeq ($(AUTO_LAYOUT),1)
OBJS = auto_main.o psp_media.o auto_probe.o probe.o capacity.o stock_reader.o layout.o card_snapshot.o fat32_init.o systemctrl_exit.o
VPATH = ../msformat
INCDIR += ../msformat
PSP_EBOOT_TITLE = PRO-X Memory Stick Auto Preview
endif
ifneq ($(filter 1,$(ATTRIBUTE_PROBE) $(IDENTITY_PROBE)),)
OBJS = attribute_main.o probe.o stock_reader.o systemctrl_exit.o
CFLAGS += -DMSDIAG_ATTRIBUTE_PROBE
PSP_EBOOT_TITLE = PRO-X Memory Stick Attribute Probe
endif
ifeq ($(IDENTITY_PROBE),1)
OBJS += identity_probe.o
CFLAGS += -DMSDIAG_IDENTITY_PROBE
PSP_EBOOT_TITLE = PRO-X Memory Stick Identity Probe
endif
ifeq ($(MEDIA_EVENT_PROBE),1)
OBJS = media_event_main.o media_session.o systemctrl_exit.o
PSP_EBOOT_TITLE = PRO-X Memory Stick Change Probe
endif
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

systemctrl_exit.o: ../../SystemControl/libs/libpspsystemctrl_kernel/SystemCtrlForKernel.S
	$(CC) $(CFLAGS) -DF_SystemCtrlForKernel_0000 -DF_SystemCtrlForKernel_0001 -c $< -o $@
