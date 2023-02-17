CROSS_COMPILE = /home/ppq/rk3308sdk/rk3308_linux_v1.00_220210/buildroot/output/rockchip_rk3308_bs_release/host/bin/aarch64-rockchip-linux-gnu-
CC = @echo "GCC $@"; $(CROSS_COMPILE)gcc
CXX = @echo "G++ $@"; $(CROSS_COMPILE)g++
RM = rm -rf
AR = ar -rcs
CP = cp -r
MKDIR = mkdir -p

TOPDIR = .

HV_PATH:=libhv
ALI_PATH:=LinkSDK-MQTT3-CA
SRC_DIRS := $(shell find src -maxdepth 3 -type d)
INC_DIRS += $(shell find ${ALI_PATH} -type d \( ! -name demos \))

LINKTOOL_PATH:=liblinktool

CFLAGS += $(addprefix -I , $(SRC_DIRS))
CFLAGS += $(addprefix -I , $(INC_DIRS))
CFLAGS += -I$(LINKTOOL_PATH)/include
CFLAGS += -I$(LINKTOOL_PATH)/include/base64
CFLAGS += -I$(LINKTOOL_PATH)/include/cJSON
CFLAGS += -I$(LINKTOOL_PATH)/include/klib
CFLAGS += -I$(LINKTOOL_PATH)/include/tcp
CFLAGS += -I$(LINKTOOL_PATH)/include/signal
CFLAGS += -I$(LINKTOOL_PATH)/include/timer
CFLAGS += -I$(LINKTOOL_PATH)/include/md5
CFLAGS += -I$(ALI_PATH)/include
CFLAGS += -I$(HV_PATH)/include/hv

CFLAGS += -Wall #-Werror
ifdef DEBUG
CFLAGS += -g -lmcheck -DDEBUG
endif

LDFLAGS += -L$(LINKTOOL_PATH)/lib
LDFLAGS += -L$(HV_PATH)/lib
LDFLAGS += -L$(ALI_PATH)
LDFLAGS += -L$(TOPDIR)

LIBS += -Wl,--start-group	\
		-Wl,-Bstatic  -llinktool -lalisdk -lzlog -lhv \
		-Wl,-Bdynamic -ldl -lm -lpthread -lrt -lsqlite3	-lcurl -lDeviceIo -lasound \
		-Wl,--end-group

SRC := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
INC:=$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.h))

OBJ += $(SRC:%.c=%.o)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

ifdef ICE
TARGET := X8Iceapp
CFLAGS += -DICE_ENABLE
else
TARGET := X8app
endif
.PHONY : all clean

all: $(TARGET)

$(TARGET) : $(OBJ)
	$(CXX) $^  $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@

clean :
	$(RM) $(TARGET)
	$(RM) $(OBJ)
