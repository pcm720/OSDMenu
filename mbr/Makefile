# If enabled, will print additional debug text to stdout
ENABLE_PRINTF ?= 0

EE_BIN = osdmbr.elf
EE_BIN_RAW = osdmbr.bin
EE_BIN_KELF = OSDMBR.XLF

EE_LINKFILE = linkfile

EE_INCS = -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/sbv/include  -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include  -Iinclude -I../common/include
EE_LIBS = -lpatches -ldma -ldebug -lfileXio -lsecr -lpad -lgskit -ldmakit -lmc
EE_OBJS =  main.o init.o config.o crypto.o loader.o hdd.o patinfo.o common.o disc.o cnf.o
ELF_FILES = loader.elf ps1vn.elf
IRX_FILES = iomanX.irx fileXio.irx ps2dev9.irx ps2atad.irx ps2hdd-osd.irx ps2fs.irx secrsif.irx

# CDROM dependencies
EE_OBJS += history.o game_id.o
RES_FILES += icon_A.sys icon_C.sys icon_J.sys

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)
EE_CFLAGS = -D_EE -Os -G0 -Wall -Werror -fdata-sections -ffunction-sections $(EE_INCS)
EE_LDFLAGS += -Wl,-zmax-page-size=128 -Wl,--gc-sections -s
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/

ifeq ($(ENABLE_PRINTF), 1)
 EE_CFLAGS += -DENABLE_PRINTF
endif

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS += $(RES_FILES:.sys=_sys.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

.PHONY: all clean

all: $(EE_BIN_RAW)

clean:
	$(MAKE) -C ../utils/loader clean
	$(MAKE) -C ../utils/ps1vn clean
	rm -rf $(EE_OBJS_DIR) $(EE_BIN) $(EE_BIN_RAW) $(EE_BIN_KELF)

# Produces KELF payload for __mbr
kelf: $(EE_BIN_RAW)
	python3 ../utils/kelf/sign.py mbr $(EE_BIN_RAW) $(EE_BIN_KELF)

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx

%ps2hdd-osd_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/ps2hdd-osd.irx $@ $(*:$(EE_SRC_DIR)%=%)ps2hdd_osd_irx

# ELF loader
loader.elf:
	$(MAKE) -C ../utils/loader/$<

%loader_elf.c: loader.elf
	$(BIN2C) $(*:$(EE_SRC_DIR)%=../utils/loader/%)loader.elf $@ $(*:$(EE_SRC_DIR)%=%)loader_elf

# PS1 Video Mode Negator
ps1vn.elf:
	$(MAKE) -C ../utils/ps1vn/$<

%ps1vn_elf.c: ps1vn.elf
	$(BIN2C) $(*:$(EE_SRC_DIR)%=../utils/ps1vn/%)ps1vn.elf $@ $(*:$(EE_SRC_DIR)%=%)ps1vn_elf

# Resource files
%_sys.c:
	$(BIN2C) ../common/res/$(*:$(EE_SRC_DIR)%=%).sys $@ $(*:$(EE_SRC_DIR)%=%)_sys

# Common files
$(EE_OBJS_DIR)history.o: | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c ../common/src/history.c -o $@
$(EE_OBJS_DIR)game_id.o: | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c ../common/src/game_id.c -o $@
$(EE_OBJS_DIR)cnf.o: | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c ../common/src/cnf.c -o $@
$(EE_OBJS_DIR)loader.o: | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c ../common/src/loader.c -o $@
$(EE_OBJS_DIR)patinfo.o: | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c ../common/src/patinfo.c -o $@

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
