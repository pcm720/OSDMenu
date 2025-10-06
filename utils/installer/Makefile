# ELF file name
EE_BIN_PKD = hosd_installer.elf
EE_BIN = hosd_installer_unc.elf

# Base object files
EE_OBJS = main.o init.o

# Base modules
IRX_FILES += iomanX.irx fileXio.irx
IRX_FILES += ps2dev9.irx ps2atad.irx ps2hdd.irx
RES_FILES = payload.bin

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/
EE_LIBS = -lpatches -ldebug -lfileXio -lpoweroff
EE_INCS = -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/sbv/include -Iinclude
EE_LDFLAGS += -s

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS += $(RES_FILES:.bin=_bin.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

# Reduce binary size by using newlib-nano
EE_NEWLIB_NANO = 1
NEWLIB_NANO = 1

.PHONY: all clean

all: $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

clean:
	$(MAKE) -C ../../mbr clean
	rm -rf $(EE_OBJS_DIR) $(EE_BIN) $(EE_BIN_PKD) res/*

BIN2C = $(PS2SDK)/bin/bin2c

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx

# OSD Version is required to write to hdd0:__mbr
%ps2hdd_irx.c: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	$(BIN2C) $(PS2SDK)/iop/irx/ps2hdd-osd.irx $@ ps2hdd_irx

# Payload to inject
%mbr.bin:
	$(MAKE) -C ../../mbr

# Sign the payload and wrap it into a .c file
%payload_bin.c: ../../mbr/mbr.bin
	@mkdir -p res
	./kelftool/kelftool encrypt mbr ../../mbr/mbr.bin res/payload.bin
	$(BIN2C) res/payload.bin $@ payload_bin

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
