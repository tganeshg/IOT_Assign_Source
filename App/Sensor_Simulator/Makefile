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
	LFLAGS  = -L./ -L${TARGET_LIB} -lmodbus
else
	INCS 	= -I./include
	LFLAGS  = -L./ -lmodbus
endif

CFLAGS	= -Wall -Wno-unused-variable -Wunused-but-set-variable -Wpointer-sign

# change these to set the proper directories where each files should be
SRCDIR   = source
INCDIR	 = include
OBJDIR   = objects
BINDIR   = bin

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(INCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm       = rm -Rf

TARGET=sensor_simulator
 
$(BINDIR)/$(TARGET): $(OBJECTS)
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
