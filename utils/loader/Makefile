EE_BIN = loader.elf
EE_OBJS = loader.o

ELF_FILES += egsm.elf
EE_LIBS = -lfileXio -liopreboot -lpatches
EE_INCS = -I../egsm/include

EE_OBJS_DIR = obj/
EE_SRC_DIR = src/
EE_LINKFILE := linkfile
EE_CFLAGS = -D_EE -Os -G0 -Wall
EE_CFLAGS += -fdata-sections -ffunction-sections
EE_LDFLAGS = -Wl,-zmax-page-size=128
EE_LDFLAGS += -Wl,--gc-sections -s
NEWLIB_NANO = 1
EE_NEWLIB_NANO = 1

EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

BIN2C = $(PS2SDK)/bin/bin2c

all: $(EE_BIN)

clean:
	$(MAKE) -C ../egsm clean
	rm -f -r $(EE_OBJS) $(EE_BIN)

# eGSM
egsm.elf:
	$(MAKE) -C ../egsm/$<

%egsm_elf.c: egsm.elf
	$(BIN2C) $(*:$(EE_SRC_DIR)%=../egsm/%)egsm.elf $@ $(*:$(EE_SRC_DIR)%=%)egsm_elf

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
