CROSS_COMPILE = arm-rockchip-linux-gnueabihf-
CC = @echo "GCC $@"; $(CROSS_COMPILE)gcc
CXX = @echo "G++ $@"; $(CROSS_COMPILE)g++
RM = rm -rf
AR = ar -rcs
CP = cp -r
MKDIR = mkdir -p

TOPDIR = .

SRC_DIRS := $(shell find src -maxdepth 3 -type d)

LINKTOOL_PATH:=liblinktool
ALI_PATH:=libalink
# LOG_PATH:=libzlog
HISTORY_PATH:=libhistory

CFLAGS += $(addprefix -I , $(SRC_DIRS))
CFLAGS += -I$(LINKTOOL_PATH)/include
CFLAGS += -I$(LINKTOOL_PATH)/include/base64
CFLAGS += -I$(LINKTOOL_PATH)/include/cJSON
CFLAGS += -I$(LINKTOOL_PATH)/include/klib
CFLAGS += -I$(LINKTOOL_PATH)/include/tcp
CFLAGS += -I$(LINKTOOL_PATH)/include/signal
CFLAGS += -I$(LINKTOOL_PATH)/include/timer
CFLAGS += -I$(ALI_PATH)/include
# CFLAGS += -I$(LOG_PATH)/include
CFLAGS += -I$(HISTORY_PATH)
CFLAGS += -Wall #-Werror
ifdef DEBUG
CFLAGS += -g -lmcheck -DDEBUG
endif

LDFLAGS += -L$(LINKTOOL_PATH)/lib
LDFLAGS += -L$(ALI_PATH)/lib
# LDFLAGS += -L$(LOG_PATH)/lib
LDFLAGS += -L$(HISTORY_PATH)
LDFLAGS += -L$(TOPDIR)

LIBS += -Wl,--start-group	\
		-Wl,-Bstatic  -llinktool -liot_hal -liot_sdk -liot_tls -lzlog -lhistory \
		-Wl,-Bdynamic -ldl -lm -lpthread -lrt -lsqlite3	-lcurl -lDeviceIo -lasound \
		-Wl,--end-group

SRC := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
INC:=$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.h))

OBJ += $(SRC:%.c=%.o)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

TARGET := X50app
.PHONY : all clean

all: $(TARGET)

$(TARGET) : $(OBJ)
	$(CXX) $^  $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@

clean :
	$(RM) $(TARGET)
	$(RM) $(OBJ)
