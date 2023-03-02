CROSS_COMPILE = /home/ppq/rk3308sdk/rk3308_linux_v1.00_220210/buildroot/output/rockchip_rk3308_bs_release/host/bin/aarch64-rockchip-linux-gnu-
CC = @echo "GCC $@"; $(CROSS_COMPILE)gcc
CXX = @echo "G++ $@"; $(CROSS_COMPILE)g++
RM = rm -rf
AR = ar -rcs
CP = cp -r
MKDIR = mkdir -p

TOPDIR = .

CFLAGS += -I/home/ppq/rk3308sdk/rk3308_linux_v1.00_220210/buildroot/output/rockchip_rk3308_bs_release/host/aarch64-rockchip-linux-gnu/sysroot/usr/include/libdrm/
CFLAGS += -Wall #-Werror
ifdef DEBUG
CFLAGS += -g -lmcheck -DDEBUG
endif

LDFLAGS += -L$(TOPDIR)

LIBS += -Wl,--start-group	\
		-Wl,-Bstatic  \
		-Wl,-Bdynamic -ldl -lm -lpthread -lrt -ldrm  \
		-Wl,--end-group

SRCFB := bmp_show.c
OBJFB += $(SRCFB:%.c=%.o)

SRCDRM := bmp_show_drm.c
OBJDRM += $(SRCDRM:%.c=%.o)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY : all clean

all: logoapp logodrmapp

logoapp : $(OBJFB)
	$(CXX) $^  $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@

logodrmapp : $(OBJDRM)
	$(CXX) $^  $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@

clean :
	$(RM) $(TARGET)
	$(RM) $(OBJFB) $(OBJDRM)
