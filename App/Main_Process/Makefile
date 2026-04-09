# For Openwrt - Raspi
# Smart Home Energy Monitor and control System @ Febraury 2026#

#cross compiler
ifeq ($(RASPI), 1)
	ROOT_DIR := /media/ganesh/New
	export PATH := ${ROOT_DIR}/Raspi/openwrt/staging_dir/toolchain-arm_arm1176jzf-s+vfp_gcc-12.3.0_musl_eabi/bin:$(PATH)
	export STAGING_DIR := ${ROOT_DIR}/Raspi/openwrt/staging_dir/
	TARGET_INCLUDE=${ROOT_DIR}/Raspi/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_musl_eabi/usr/include
	TARGET_LIB=${ROOT_DIR}/Raspi/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_musl_eabi/usr/lib
	CC = arm-openwrt-linux-gcc
	INCS 	= -I./include -I${TARGET_INCLUDE}
	LFLAGS  = -L./ -L${TARGET_LIB} -lmodbus -lz -lcares -lsqlite3 -lssl -lcrypto -lmosquitto -llvgl -lm -lpthread
else
	INCS 	= -I./include
	LFLAGS  = -L./ -lmodbus -lsqlite3 -lmosquitto -llvgl -lm -lpthread
endif

# LVGL headers/libs: use pkg-config when available (typical on target SDK / desktop dev packages).
LVGL_PKG_CFLAGS := $(shell pkg-config --cflags lvgl 2>/dev/null)
LVGL_PKG_LIBS := $(shell pkg-config --libs lvgl 2>/dev/null)
ifneq ($(strip $(LVGL_PKG_LIBS)),)
INCS += $(LVGL_PKG_CFLAGS)
LFLAGS := $(filter-out -llvgl,$(LFLAGS))
LFLAGS += $(LVGL_PKG_LIBS)
endif

CFLAGS	= -Wall -Wno-unused-variable -Wunused-but-set-variable -Wpointer-sign -DENABLE_LVGL

# change these to set the proper directories where each files should be
SRCDIR   = source
INCDIR	 = include
OBJDIR   = objects
BINDIR   = ../AppBin

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(INCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm       = rm -Rf

TARGET=mainProc

.PHONY: all
all: $(BINDIR)/$(TARGET)

$(BINDIR):
	@mkdir -p $(BINDIR)

$(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
	@$(CC) -o  $@ $(OBJECTS) $(LFLAGS)
	@#cp $(TARGET) ../Firmware/bin
	@echo "Linking completed!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(INCS) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

.PHONEY: clean
clean:
	@$(rm) $(OBJDIR)/*.o
	@$(rm) $(BINDIR)/$(TARGET)
	@echo $(OBJECTS)
	@echo $(TARGET)
	@echo "Cleanup completed!"
