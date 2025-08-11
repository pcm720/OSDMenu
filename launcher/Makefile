# Launcher configuration
# Support for handling OSDMenu arguments
FMCB ?= 1
# Support for loading ELFs from MMCE (mmce?:)
MMCE ?= 1
# Support for loading ELFs from USB (usb:/mass:)
USB ?= 1
# Support for loading ELFs from exFAT internal HDD (ata:)
ATA ?= 1
# Support for loading ELFs from MX4SIO (mx4sio:)
MX4SIO ?= 1
# Support for loading ELFs from iLink (ilink:)
ILINK ?= 1
# Support for loading ELFs from UDPBD (udpbd:)
UDPBD ?= 1
# Support for loading ELFs from APA-formatted HDD (hdd0:)
APA ?= 1
# Support for loading PS1 and PS2 CDs/DVDs (cdrom)
CDROM ?= 1

# Size reduction options
# If enabled, will use SIO2MAN, MCMAN and MCSERV from rom0 instead of PS2SDK modules
USE_ROM_MODULES ?= 0
# If enabled, will print additional debug test to stdout
ENABLE_PRINTF ?= 0

# End of configurable section

# ELF file name
EE_BIN_PKD = launcher.elf
EE_BIN = launcher_unc.elf

# Base object files
EE_OBJS = main.o common.o init.o loader.o
EE_OBJS += handler_mc.o handler_quickboot.o

# Base modules
IRX_FILES += iomanX.irx fileXio.irx
ELF_FILES += loader.elf

ifeq ($(FMCB), 1)
 CDROM = 1
 EE_CFLAGS += -DFMCB
 EE_OBJS += handler_fmcb.o
 IRX_FILES += poweroff.irx
endif

# Additional device support
ifeq ($(MMCE), 1)
 SIO2MAN = 1
 EE_CFLAGS += -DMMCE
 IRX_FILES += mmceman.irx
endif

ifeq ($(USB), 1)
 BDM = 1
 EE_CFLAGS += -DUSB
 IRX_FILES += usbd_mini.irx usbmass_bd_mini.irx
endif

ifeq ($(ATA), 1)
 BDM = 1
 DEV9 = 1
 EE_CFLAGS += -DATA
 IRX_FILES += ata_bd.irx
endif

ifeq ($(MX4SIO), 1)
 SIO2MAN = 1
 BDM = 1
 EE_CFLAGS += -DMX4SIO
 IRX_FILES += mx4sio_bd_mini.irx
endif

ifeq ($(ILINK), 1)
 BDM = 1
 EE_CFLAGS += -DILINK
 IRX_FILES += iLinkman.irx IEEE1394_bd_mini.irx
endif

ifeq ($(UDPBD), 1)
 BDM = 1
 DEV9 = 1
 EE_CFLAGS += -DUDPBD
 IRX_FILES += smap_udpbd.irx
endif

ifeq ($(APA), 1)
 DEV9 = 1
 EE_CFLAGS += -DAPA
 EE_OBJS += handler_pfs.o
 IRX_FILES += ps2atad.irx ps2hdd-osd.irx ps2fs.irx
endif

ifeq ($(CDROM),1)
 EE_CFLAGS += -DCDROM
 EE_OBJS += handler_cdrom.o history.o game_id.o
 RES_FILES += icon_A.sys icon_C.sys icon_J.sys
 IRX_FILES += xparam.irx
 ELF_FILES += ps1vn.elf
endif

# Driver dependencies
ifeq ($(SIO2MAN), 1)
 IRX_FILES += sio2man.irx
endif

ifeq ($(DEV9), 1)
 IRX_FILES += ps2dev9.irx
endif

ifeq ($(BDM), 1)
 EE_OBJS += handler_bdm.o
 IRX_FILES += bdm.irx bdmfs_fatfs.irx
endif

# Size reduction flags
ifeq ($(USE_ROM_MODULES), 1)
 EE_CFLAGS += -DUSE_ROM_MODULES
else
 IRX_FILES += mcman.irx mcserv.irx
endif

ifeq ($(ENABLE_PRINTF), 1)
 EE_CFLAGS += -DENABLE_PRINTF
endif

# Custom paths
ifdef CONF_PATH
 EE_CFLAGS += -DCONF_PATH=\"$(CONF_PATH)\"
endif
ifdef LAUNCHER_PATH
 EE_CFLAGS += -DLAUNCHER_PATH=\"$(LAUNCHER_PATH)\"
endif
ifdef DKWDRV_PATH
 EE_CFLAGS += -DDKWDRV_PATH=\"$(DKWDRV_PATH)\"
endif

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/
EE_LIBS = -lcdvd -lpatches -lkernel -ldebug -lfileXio -lgskit -ldmakit -lmc -lpoweroff
EE_INCS = -I../common -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/sbv/include -Iinclude -I$(PS2SDK)/ports/include -I$(PS2DEV)/gsKit/include
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib -s

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS += $(RES_FILES:.sys=_sys.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

# Reduce binary size by using newlib-nano
EE_NEWLIB_NANO = 1
NEWLIB_NANO = 1

.PHONY: all clean

all: $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

clean:
	$(MAKE) -C ../utils/loader clean
	$(MAKE) -C ../utils/ps1vn clean
	$(MAKE) -C iop/xparam clean
	$(MAKE) -C iop/smap_udpbd clean
	rm -rf $(EE_OBJS_DIR) $(EE_BIN) $(EE_BIN_PKD)

BIN2C = $(PS2SDK)/bin/bin2c

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx

%ps2hdd-osd_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/ps2hdd-osd.irx $@ $(*:$(EE_SRC_DIR)%=%)ps2hdd_osd_irx

# smap_udpbd.irx
iop/smap_udpbd/smap_udpbd.irx: iop/smap_udpbd
	$(MAKE) -C $<

%smap_udpbd_irx.c: iop/smap_udpbd/smap_udpbd.irx
	$(BIN2C) iop/smap_udpbd/$(*:$(EE_SRC_DIR)%=%)smap_udpbd.irx $@ $(*:$(EE_SRC_DIR)%=%)smap_udpbd_irx

# xparam.irx
iop/xparam/xparam.irx: iop/xparam
	$(MAKE) -C $<

%xparam_irx.c: iop/xparam/xparam.irx
	$(BIN2C) iop/xparam/$(*:$(EE_SRC_DIR)%=%)xparam.irx $@ $(*:$(EE_SRC_DIR)%=%)xparam_irx

# mmceman module, temporary override until it lands in the SDK
%mmceman_irx.c:
	$(BIN2C) iop/mmceman/mmceman.irx $@ $(*:$(EE_SRC_DIR)%=%)mmceman_irx

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
	$(BIN2C) res/$(*:$(EE_SRC_DIR)%=%).sys $@ $(*:$(EE_SRC_DIR)%=%)_sys

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
