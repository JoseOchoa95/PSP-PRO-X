TARGET = mswrite
OBJS = main.o probe.o stock_reader.o systemctrl_exit.o
VPATH = ../msdiag
INCDIR = ../../include ../msdiag
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DMSDIAG_WRITE_TRIAL -DMSWRITE_BUILD_ID=\"$(BUILD_ID)\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBS = -lpspctrl_driver
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X Memory Stick Write Trial
ifeq ($(PARTITION_TRIAL),1)
TARGET = mspart
OBJS += partition_table.o
CFLAGS += -DMSPART_TRIAL
PSP_EBOOT_TITLE = PRO-X Memory Stick Partition Trial
endif
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

systemctrl_exit.o: ../../SystemControl/libs/libpspsystemctrl_kernel/SystemCtrlForKernel.S
	$(CC) $(CFLAGS) -DF_SystemCtrlForKernel_0000 -DF_SystemCtrlForKernel_0001 -c $< -o $@
