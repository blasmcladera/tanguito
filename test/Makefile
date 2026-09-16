########################################################################
# BSD 3-Clause License
# Copyright (c) 2017, Martin Ribelotta and Eric Pernia
# All rights reserved.
#
# Adapted for EDU-CIAA-MEF project
########################################################################


# Board ----------------------------------------------------------------

BOARD = edu_ciaa_nxp

-include board.mk


# Program --------------------------------------------------------------

PROGRAM_NAME = app

-include program.mk


# Compilation and libraries configurations -----------------------------

VERBOSE = n
OPT = g
USE_NANO = y
USE_LTO = n
SEMIHOST = n
USE_FPU = y
ENFORCE_NOGPL = n
M0_APP =


# Libraries -------------------------------------------------------------

USE_LPCOPEN = y
USE_SAPI = y
USE_M0 = n


-include config.mk


# ----------------------------------------------------------------------
# Directories
# ----------------------------------------------------------------------

MODULES := $(notdir $(wildcard libs/*))
COMPONENTS := $(notdir $(wildcard components/*))


# ----------------------------------------------------------------------
# Source files
# ----------------------------------------------------------------------

SRC := $(wildcard *.c)

SRC += $(foreach m,$(MODULES),$(wildcard libs/$(m)/src/*.c))

SRC += $(foreach c,$(COMPONENTS),$(wildcard components/$(c)/src/*.c))


CXXSRC := $(wildcard *.cpp)

CXXSRC += $(foreach m,$(MODULES),$(wildcard libs/$(m)/src/*.cpp))

CXXSRC += $(foreach c,$(COMPONENTS),$(wildcard components/$(c)/src/*.cpp))


INOSRC := $(wildcard *.ino)

INOSRC += $(foreach m,$(MODULES),$(wildcard libs/$(m)/src/*.ino))

INOSRC += $(foreach c,$(COMPONENTS),$(wildcard components/$(c)/src/*.ino))


ASRC := $(wildcard *.s)

ASRC += $(foreach m,$(MODULES),$(wildcard libs/$(m)/src/*.s))

ASRC += $(foreach c,$(COMPONENTS),$(wildcard components/$(c)/src/*.s))


ASSRC := $(wildcard *.sx)

ASSRC += $(foreach m,$(MODULES),$(wildcard libs/$(m)/src/*.sx))

ASSRC += $(foreach c,$(COMPONENTS),$(wildcard components/$(c)/src/*.sx))


ASSSRC := $(wildcard *.S)

ASSSRC += $(foreach m,$(MODULES),$(wildcard libs/$(m)/src/*.S))

ASSSRC += $(foreach c,$(COMPONENTS),$(wildcard components/$(c)/src/*.S))


# ----------------------------------------------------------------------
# Output directory
# ----------------------------------------------------------------------

OUT = out


# ----------------------------------------------------------------------
# Objects
# ----------------------------------------------------------------------

OBJECTS = $(INOSRC:%.ino=$(OUT)/%.o)

OBJECTS += $(CXXSRC:%.cpp=$(OUT)/%.o)

OBJECTS += $(SRC:%.c=$(OUT)/%.o)

OBJECTS += $(ASRC:%.s=$(OUT)/%.o)

OBJECTS += $(ASSRC:%.sx=$(OUT)/%.o)

OBJECTS += $(ASSSRC:%.S=$(OUT)/%.o)


# ----------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------

DEPS = $(OBJECTS:%.o=%.d)


# ----------------------------------------------------------------------
# Output files
# ----------------------------------------------------------------------

TARGET = $(OUT)/$(PROGRAM_NAME).elf

TARGET_BIN = $(basename $(TARGET)).bin

TARGET_HEX = $(basename $(TARGET)).hex

TARGET_LST = $(basename $(TARGET)).lst

TARGET_MAP = $(basename $(TARGET)).map

TARGET_NM = $(basename $(TARGET)).names.csv

TARGET_ELF = $(basename $(TARGET)).elf

TARGET_AXF = $(basename $(TARGET)).axf


# ----------------------------------------------------------------------
# Include paths
# ----------------------------------------------------------------------

INCLUDE_FLAGS =

# Includes de las librerías originales
INCLUDE_FLAGS += $(foreach m,$(MODULES),-Ilibs/$(m)/inc)

# Includes de los componentes propios
INCLUDE_FLAGS += $(foreach c,$(COMPONENTS),-Icomponents/$(c)/include)

# Includes del directorio raíz
INCLUDE_FLAGS += -I.

# Includes adicionales
INCLUDE_FLAGS += $(INCLUDES)


# ----------------------------------------------------------------------
# Defines
# ----------------------------------------------------------------------

DEFINES_FLAGS = $(foreach m,$(DEFINES),-D$(m))


# ----------------------------------------------------------------------
# Optimization
# ----------------------------------------------------------------------

OPT_FLAGS = -ggdb3 -O$(OPT) -ffunction-sections -fdata-sections


# ----------------------------------------------------------------------
# Library dependencies
# ----------------------------------------------------------------------

LIBSDEPS = $(addprefix $(OUT)/, \
	$(addsuffix .a, \
		$(basename \
			$(foreach l,$(LIBS), \
				$(foreach m,$(MODULES), \
					$(wildcard libs/$(m)/lib/lib$(l).hexlib) \
				) \
			) \
		) \
	) \
)


# ----------------------------------------------------------------------
# Common compilation flags
# ----------------------------------------------------------------------

COMMON_FLAGS = $(ARCH_FLAGS) \
	$(DEFINES_FLAGS) \
	$(INCLUDE_FLAGS) \
	$(OPT_FLAGS) \
	-DBOARD=$(BOARD)


# ----------------------------------------------------------------------
# C flags
# ----------------------------------------------------------------------

CFLAGS = $(COMMON_FLAGS) -std=c99


# ----------------------------------------------------------------------
# C++ flags
# ----------------------------------------------------------------------

CXXFLAGS = $(COMMON_FLAGS) -fno-rtti -fno-exceptions -std=c++11


# ----------------------------------------------------------------------
# Arduino flags
# ----------------------------------------------------------------------

INOFLAGS = $(CXXFLAGS) -x c++ -include Arduino.h


# ----------------------------------------------------------------------
# Linker flags
# ----------------------------------------------------------------------

LDFLAGS = $(ARCH_FLAGS)


LDFLAGS += $(addprefix -L, \
	$(foreach m,$(MODULES), \
		$(wildcard libs/$(m)/lib) \
	) \
)


LDFLAGS += $(addprefix -L, $(wildcard $(dir $(LIBSDEPS))))


LDFLAGS += $(addprefix -l,$(LIBS))


LDFLAGS += -T$(LDSCRIPT)


LDFLAGS += -nostartfiles \
	-Wl,-gc-sections \
	-Wl,-Map=$(TARGET_MAP) \
	-Wl,--cref


# ----------------------------------------------------------------------
# Nano libc
# ----------------------------------------------------------------------

ifeq ($(USE_NANO),y)

LDFLAGS += --specs=nano.specs

endif


# ----------------------------------------------------------------------
# Link Time Optimization
# ----------------------------------------------------------------------

ifeq ($(USE_LTO),y)

$(info Using LTO)

ifeq ($(OPT),g)

$(warning "Using LTO in debug may cause inconsistences in debug")

endif

COMMON_FLAGS += -flto

LDFLAGS += -flto

endif


# ----------------------------------------------------------------------
# Semihosting
# ----------------------------------------------------------------------

ifeq ($(SEMIHOST),y)

$(info Using semihosting)

DEFINES += USE_SEMIHOST

LDFLAGS += --specs=rdimon.specs

endif


# ----------------------------------------------------------------------
# Toolchain
# ----------------------------------------------------------------------

CROSS = arm-none-eabi-

CC = $(CROSS)gcc

CXX = $(CROSS)g++


ifeq ($(CXXSRC),)

LD = $(CROSS)gcc

else

LD = $(CROSS)g++

endif


SIZE = $(CROSS)size

LIST = $(CROSS)objdump -xdS

OBJCOPY = $(CROSS)objcopy

NM = $(CROSS)nm

GDB = $(CROSS)gdb


# ----------------------------------------------------------------------
# Verbose mode
# ----------------------------------------------------------------------

ifeq ($(VERBOSE),y)

Q =

else

Q = @

endif


# ----------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------

all: .m0app $(OUT) .try_enforce_no_gpl $(TARGET) $(TARGET_BIN) $(TARGET_HEX) $(TARGET_LST) $(TARGET_NM) size

	@echo
	@echo Selected program: $(PROGRAM_NAME)
	@echo Selected board: $(BOARD)


# ----------------------------------------------------------------------
# M0 application
# ----------------------------------------------------------------------

ifeq ($(M0_APP),)

.m0app: ;

.m0app_clean: ;

else

.m0app:

	@echo M0 APP: PROGRAM_NAME=$(notdir $(M0_APP))

	@$(MAKE) PROGRAM_NAME=$(notdir $(M0_APP)) USE_M0=y all


.m0app_clean:

	@echo M0 APP CLEAN: PROGRAM_NAME=$(notdir $(M0_APP))

	@$(MAKE) PROGRAM_NAME=$(notdir $(M0_APP)) USE_M0=y clean

endif


# ----------------------------------------------------------------------
# Module configuration files
# ----------------------------------------------------------------------

-include $(foreach m,$(MODULES),$(wildcard libs/$(m)/module.mk))


# ----------------------------------------------------------------------
# Dependency files
# ----------------------------------------------------------------------

-include $(DEPS)


# ----------------------------------------------------------------------
# Output directory
# ----------------------------------------------------------------------

$(OUT):

	@mkdir -p $@


# ----------------------------------------------------------------------
# C compilation
# ----------------------------------------------------------------------

$(OUT)/%.o: %.c

	@echo CC $(notdir $<)

	@mkdir -p $(dir $@)

	$(Q)$(CC) -MMD $(CFLAGS) -c -o $@ $<


# ----------------------------------------------------------------------
# C++ compilation
# ----------------------------------------------------------------------

$(OUT)/%.o: %.cpp

	@echo CXX $(notdir $<)

	@mkdir -p $(dir $@)

	$(Q)$(CXX) -MMD $(CXXFLAGS) -c -o $@ $<


# ----------------------------------------------------------------------
# Arduino compilation
# ----------------------------------------------------------------------

$(OUT)/%.o: %.ino

	@echo ARDUINO CXX $(notdir $<)

	@mkdir -p $(dir $@)

	$(Q)$(CXX) -MMD $(INOFLAGS) -c -o $@ $<


# ----------------------------------------------------------------------
# Assembly .s
# ----------------------------------------------------------------------

$(OUT)/%.o: %.s

	@echo AS $(notdir $<)

	@mkdir -p $(dir $@)

	$(Q)$(CC) -MMD $(CFLAGS) -c -o $@ $<


# ----------------------------------------------------------------------
# Assembly .sx
# ----------------------------------------------------------------------

$(OUT)/%.o: %.sx

	@echo AS $(notdir $<)

	@mkdir -p $(dir $@)

	$(Q)$(CC) -MMD $(CFLAGS) -c -o $@ $<


# ----------------------------------------------------------------------
# Assembly .S
# ----------------------------------------------------------------------

$(OUT)/%.o: %.S

	@echo AS $<

	@mkdir -p $(dir $@)

	$(Q)$(CC) -MMD $(CFLAGS) -c -o $@ $<


# ----------------------------------------------------------------------
# Libraries
# ----------------------------------------------------------------------

$(OUT)/%.a: %.hexlib

	@echo DEBLOB $(notdir $<)

	@mkdir -p $(dir $@)

	$(Q)$(OBJCOPY) -I ihex -O binary $< $@


# ----------------------------------------------------------------------
# Linking
# ----------------------------------------------------------------------

$(OUT)/linker-params: $(OBJECTS) $(LIBSDEPS) Makefile

	@echo LD params

	@mkdir -p $(dir $@)

	$(Q)echo "-Wl,-( $(OBJECTS) -Wl,-) $(LDFLAGS)" > $@


$(TARGET): $(OUT)/linker-params

	@echo LD $@...

	$(Q)$(LD) -o $@ @$(OUT)/linker-params


# ----------------------------------------------------------------------
# Binary
# ----------------------------------------------------------------------

$(TARGET_BIN): $(TARGET)

	@echo COPY $(notdir $<) TO $(notdir $@)

	@mkdir -p $(dir $@)

	$(Q)$(OBJCOPY) -O binary $< $@


# ----------------------------------------------------------------------
# HEX
# ----------------------------------------------------------------------

$(TARGET_HEX): $(TARGET)

	@echo COPY $(notdir $<) TO $(notdir $@)

	@mkdir -p $(dir $@)

	$(Q)$(OBJCOPY) -O ihex $< $@


# ----------------------------------------------------------------------
# LIST
# ----------------------------------------------------------------------

$(TARGET_LST): $(TARGET)

	@echo LIST

	$(Q)$(LIST) $< > $@


# ----------------------------------------------------------------------
# Names
# ----------------------------------------------------------------------

$(TARGET_NM): $(TARGET)

	@echo NAME

	$(Q)$(NM) -nAsSCp $< > $@


# ----------------------------------------------------------------------
# Size
# ----------------------------------------------------------------------

size: $(TARGET)

	@echo SIZEOF $(notdir $<)...

	$(Q)$(SIZE) $<


# ----------------------------------------------------------------------
# Information
# ----------------------------------------------------------------------

.info:

	LANG=C $(MAKE) -B -p -r -n


# ----------------------------------------------------------------------
# AXF to BIN
# ----------------------------------------------------------------------

.axf_to_bin:

	@echo Create $(PROGRAM_NAME).bin from $(PROGRAM_NAME).axf

	arm-none-eabi-objcopy -O binary $(TARGET_AXF) $(TARGET_BIN)


# ----------------------------------------------------------------------
# ELF to BIN
# ----------------------------------------------------------------------

.elf_to_bin:

	@echo Create $(PROGRAM_NAME).bin from $(PROGRAM_NAME).elf

	arm-none-eabi-objcopy -O binary $(TARGET_ELF) $(TARGET_BIN)


# ----------------------------------------------------------------------
# OpenOCD
# ----------------------------------------------------------------------

OOCD = openocd

OOCD_SCRIPT = scripts/openocd/lpc4337.cfg


# ----------------------------------------------------------------------
# Download to M0 flash
# ----------------------------------------------------------------------

ifeq ($(M0_APP),)

.download_flash_m0: ;

else

.download_flash_m0: $(TARGET_BIN)

	@echo DOWNLOAD to M0 FLASH

	$(Q)$(OOCD) -f $(OOCD_SCRIPT) \
		-c "init" \
		-c "halt 0" \
		-c "flash write_image erase $< 0x1B000000 bin" \
		-c "reset run" \
		-c "shutdown" 2>&1

endif


# ----------------------------------------------------------------------
# Download to flash
# ----------------------------------------------------------------------

.download_flash: $(TARGET_BIN) .download_flash_m0

	@echo DOWNLOAD to FLASH

	$(Q)$(OOCD) -f $(OOCD_SCRIPT) \
		-c "init" \
		-c "halt 0" \
		-c "flash write_image erase $< 0x1A000000 bin" \
		-c "reset run" \
		-c "shutdown" 2>&1


# ----------------------------------------------------------------------
# Download to RAM
# ----------------------------------------------------------------------

.download_ram: $(TARGET_BIN)

	@echo DOWNLOAD to RAM

	$(Q)$(OOCD) -f $(OOCD_SCRIPT) \
		-c "init" \
		-c "halt 0" \
		-c "load_image $< 0x20000000 bin" \
		-c "reset run" \
		-c "shutdown" 2>&1


# ----------------------------------------------------------------------
# Download
# ----------------------------------------------------------------------

ifeq ($(LOAD_INRAM),y)

download: .download_ram

else

download: .download_flash

endif


# ----------------------------------------------------------------------
# Erase flash
# ----------------------------------------------------------------------

erase:

	@echo ERASE FLASH

	$(Q)$(OOCD) -f $(OOCD_SCRIPT) \
		-c "init" \
		-c "halt 0" \
		-c "flash erase_sector 0 0 last" \
		-c "shutdown" 2>&1

	@echo
	@echo Done.
	@echo Please reset your Board.


# ----------------------------------------------------------------------
# Debug
# ----------------------------------------------------------------------

.debug:

	@echo DEBUG

	$(Q)$(OOCD) -f $(OOCD_SCRIPT) 2>&1


.run_gdb: $(TARGET)

	$(Q)socketwaiter :3333 && gdbfront $(TARGET) --start \
	--gdb arm-none-eabi-gdb \
	--gdbcmd="target remote :3333" \
	--gdbcmd="monitor reset halt" \
	--gdbcmd="load" \
	--gdbcmd="break main" \
	--gdbcmd="continue"


debug:

	@$(MAKE) -j 2 .debug .run_gdb


# ----------------------------------------------------------------------
# Hardware test
# ----------------------------------------------------------------------

.hardware_test: $(TARGET)

	$(Q)$(OOCD) -f $(OOCD_SCRIPT) > $(TARGET).log &

	$(Q)sleep 3 && arm-none-eabi-gdb -batch $(TARGET) -x scripts/openocd/gdbinit

	$(Q)cat $(TARGET).log

	$(Q)cat $(TARGET).log | grep FAIL -o >/dev/null && exit 1 || exit 0


# ----------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------

clean: .m0app_clean

	@echo CLEAN

	$(Q)rm -fR $(OBJECTS) $(TARGET) $(TARGET_BIN) $(TARGET_LST) $(TARGET_HEX) $(TARGET_NM) $(DEPS) $(OUT)

	@echo
	@echo Clean program: $(PROGRAM_NAME)
	@echo Board: $(BOARD)


# ----------------------------------------------------------------------
# Clean all
# ----------------------------------------------------------------------

clean_all:

	@echo CLEAN ALL

	@sh scripts/program/clean_all.sh


# ----------------------------------------------------------------------
# Utilities
# ----------------------------------------------------------------------

new_program:

	@sh scripts/program/new_program.sh


select_program:

	@sh scripts/program/select_program.sh


select_board:

	@sh scripts/board/select_board.sh


# ----------------------------------------------------------------------
# Test all programs
# ----------------------------------------------------------------------

.test_build_all:

	@sh scripts/test/test-build-all.sh


# ----------------------------------------------------------------------
# GPL check
# ----------------------------------------------------------------------

$(OUT)/gpl_check.txt:

	@mkdir -p $(OUT)

	@grep -lE 'terms of the GNU (Lesser )?General Public License' \
		$(CXXSRC) $(ASRC) $(SRC) > $@ || true


.enforce_no_gpl: $(OUT)/gpl_check.txt

	@echo "CHECKING (L)GPL code in your project... "

	@[[ $(shell < $< wc -l) -ne 0 ]] && \
		echo "POSITIVE: GPL code in your project. You can see afected files in $<" || \
		echo "NEGATIVE: No GPL code in your project"


ifeq ($(ENFORCE_NOGPL),y)

.try_enforce_no_gpl: .enforce_no_gpl

else

.try_enforce_no_gpl:

endif


# ----------------------------------------------------------------------
# Eclipse configuration
# ----------------------------------------------------------------------

.print_eclipse_cfg:

	@echo $(CC) $(CFLAGS) -E -P -v -dD \"$$\{INPUTS\}\"


.gen_eclipse_cfg: Makefile

	@mkdir -p .settings

	@echo "$(CC) $(CFLAGS) -E -P -v -dD &quot;$${INPUTS}&quot;" > .settings/language.settings.xml

	@echo Settings writed to .settings/language.settings.xml


# ----------------------------------------------------------------------
# Phony targets
# ----------------------------------------------------------------------

.PHONY: all size download erase clean clean_all new_program select_program select_board