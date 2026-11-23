TARGET = native_install_preview
OBJS = main.o completion_bridge.o
INCDIR = ../../Common ../../include
CFLAGS = -Os -G0 -Wall -Wextra -Werror
BUILD_PRX = 1
PSP_FW_VERSION = 661
LIBDIR = ../../libs
LIBS = -lpspkubridge -lpsppower
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
