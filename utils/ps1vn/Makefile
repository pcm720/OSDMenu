EE_BIN = ps1vn.elf

EE_INCS = -I./include -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include

EE_LINKFILE = linkfile
EE_OBJS := main.o patches.o

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)
EE_LDFLAGS += -Wl,-zmax-page-size=128 -Wl,--gc-sections -s
EE_CFLAGS += -fdata-sections -ffunction-sections

# Reduce binary size by using newlib-nano
EE_NEWLIB_NANO = 1
NEWLIB_NANO = 1

EE_OBJS_DIR = obj/
EE_SRC_DIR = src/

EE_OBJS += $(IRX_FILES:.c=.o)
EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

.PHONY: all clean

all: $(EE_BIN)

clean:
	rm -rf $(EE_OBJS_DIR) $(EE_BIN)

BIN2C = $(PS2SDK)/bin/bin2c

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
