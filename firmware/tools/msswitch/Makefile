TARGET = msswitch
OBJS = main.o partition_view.o probe.o stock_reader.o systemctrl_exit.o
VPATH = ../msdiag
INCDIR = ../../include ../msdiag ../mswrite
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DMSSWITCH_BUILD_ID=\"$(BUILD_ID)\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBS = -lpspctrl_driver
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X Partition Switch Trial
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

systemctrl_exit.o: ../../SystemControl/libs/libpspsystemctrl_kernel/SystemCtrlForKernel.S
	$(CC) $(CFLAGS) -DF_SystemCtrlForKernel_0000 -DF_SystemCtrlForKernel_0001 -c $< -o $@
