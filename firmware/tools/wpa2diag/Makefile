TARGET = wpa2diag
OBJS = main.o ../../Common/wpa2_patch.o
INCDIR = ../../Common ../../include
CFLAGS = -Os -G0 -Wall
ASFLAGS = $(CFLAGS)
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBDIR = ../../libs
LIBS = -lpspsystemctrl_kernel -lpsputility -lpspdebug -lpspctrl_driver
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X WPA2 identification (read only)
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
