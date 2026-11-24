TARGET = flash_capacity
OBJS = main.o prox_flash_capacity.o prox_flash0_capacity.o
INCDIR = ../../Common
CFLAGS = -Os -G0 -Wall -Wextra -Werror
BUILD_PRX = 1
USE_KERNEL_LIBS = 1
USE_KERNEL_LIBC = 1
LDFLAGS = -nostartfiles
PSP_FW_VERSION = 661
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

prox_flash_capacity.o: ../../Common/prox_flash_capacity.c
	$(CC) $(CFLAGS) -c $< -o $@

prox_flash0_capacity.o: ../../Common/prox_flash0_capacity.c
	$(CC) $(CFLAGS) -c $< -o $@
