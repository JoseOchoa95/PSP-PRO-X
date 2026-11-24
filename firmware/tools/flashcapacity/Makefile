TARGET = prox_flash_capacity
OBJS = main.o ../../Common/prox_flash_capacity.o systemctrl_exit.o
INCDIR = ../../Common ../../include
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DPROX_CAPACITY_BUILD=\"$(BUILD_ID)\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBS = -lpspdebug -lpspctrl_driver
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X Flash0 Capacity
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

systemctrl_exit.o: ../../SystemControl/libs/libpspsystemctrl_kernel/SystemCtrlForKernel.S
	$(CC) $(CFLAGS) -DF_SystemCtrlForKernel_0000 -DF_SystemCtrlForKernel_0001 -c $< -o $@
