TARGET = netdiag
OBJS = main.o protocol.o
CFLAGS = -Os -G0 -Wall -Wextra -Werror -DNETDIAG_BUILD_ID=\"$(BUILD_ID)\"
BUILD_PRX = 1
PSP_FW_VERSION = 661
# build.mak supplies the network/utility libraries in SDK import-stub order.
LIBS = -lpsphttp -lpspwlan
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PRO-X Network Diagnostic 1.0
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
