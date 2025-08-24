EE_BIN_PKD = mbrboot.elf
EE_BIN = mbrboot_unc.elf

EE_LINKFILE = linkfile

EE_INCS = -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/sbv/include -Iinclude -I$(PS2SDK)/ports/include -I../../common
EE_LIBS = -lpatches -ldma -ldebug -lfileXio -lsecr -lpad
EE_OBJS =  main.o init.o config.o mbrboot.o mbrboot_crypto.o loader.o hdd.o common.o
ELF_FILES = loader.elf
IRX_FILES = iomanX.irx fileXio.irx ps2dev9.irx ps2atad.irx ps2hdd-osd.irx ps2fs.irx secrsif.irx

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)
EE_CFLAGS = -D_EE -Os -G0 -Wall -Werror -fdata-sections -ffunction-sections $(EE_INCS)
EE_LDFLAGS += -Wl,-zmax-page-size=128 -Wl,--gc-sections # -s

# Reduce binary size by using newlib-nano
EE_NEWLIB_NANO = 1
NEWLIB_NANO = 1

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

.PHONY: all clean

all: $(EE_BIN_PKD)

clean:
	$(MAKE) -C ../../patcher clean
	rm -rf $(EE_OBJS_DIR) $(EE_BIN) $(EE_BIN_PKD)

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx

%ps2hdd-osd_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/ps2hdd-osd.irx $@ $(*:$(EE_SRC_DIR)%=%)ps2hdd_osd_irx

# ELF loader
loader.elf:
	$(MAKE) -C ../loader/$<

%loader_elf.c: loader.elf
	$(BIN2C) $(*:$(EE_SRC_DIR)%=../loader/%)loader.elf $@ $(*:$(EE_SRC_DIR)%=%)loader_elf

BIN2C = $(PS2SDK)/bin/bin2c

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_BIN_RAW): $(EE_BIN)
	$(EE_OBJCOPY) -O binary -v $< $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
