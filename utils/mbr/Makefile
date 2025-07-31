PAYLOAD ?= patcher.elf

EE_BIN = mbr.elf
EE_BIN_RAW = payload.bin

EE_LINKFILE = linkfile

EE_INCS = -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/sbv/include -Iinclude -I$(PS2SDK)/ports/include
EE_LIBS = -lpatches -ldma
EE_OBJS = main.o crt0.o

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)
EE_CFLAGS = -D_EE -Os -G0 -Wall -Werror -fdata-sections -ffunction-sections $(EE_INCS)
EE_LDFLAGS += -Wl,-zmax-page-size=128 -Wl,--gc-sections -nostartfiles -s

# Reduce binary size by using newlib-nano
EE_NEWLIB_NANO = 1
NEWLIB_NANO = 1

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
ifneq (,$(findstring ELF,$(PAYLOAD)))
	EE_OBJS += $(PAYLOAD:.ELF=_ELF.o)
else
	EE_OBJS += $(PAYLOAD:.elf=_elf.o)
endif
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

.PHONY: all clean

all: $(EE_BIN_RAW)

clean:
	$(MAKE) -C ../../patcher clean
	rm -rf $(EE_OBJS_DIR) $(EE_BIN) $(EE_BIN_RAW)

# HOSDMenu payload
%patcher_elf.c:
	$(MAKE) -C ../../patcher clean
	$(MAKE) -C ../../patcher HOSD=1
	$(BIN2C) ../../patcher/patcher.elf $@ payload_elf

# Custom payload
%_elf.c:
	$(BIN2C) $(*:$(EE_SRC_DIR)%=%).elf $@ payload_elf

%_ELF.c:
	$(BIN2C) $(*:$(EE_SRC_DIR)%=%).ELF $@ payload_elf

BIN2C = $(PS2SDK)/bin/bin2c

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
