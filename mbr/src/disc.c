#include "cnf.h"
#include "common.h"
#include "config.h"
#include "defaults.h"
#include "dprintf.h"
#include "game_id.h"
#include "hdd.h"
#include "history.h"
#include "loader.h"
#include <ctype.h>
#include <fcntl.h>
#include <kernel.h>
#include <libcdvd.h>
#include <osd_config.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PS1 Video Mode Negator variables
extern uint8_t ps1vn_elf[];
extern int size_ps1vn_elf;

// PS1DRV flags
#define CDROM_PS1_FAST 0x1
#define CDROM_PS1_SMOOTH 0x10

// Boots PS1 disc using PS1DRV or DKWDRV
void handlePS1Disc(char *titleID, char *titleVersion);
// Boots PS2 disc directly or via PS2LOGO
void handlePS2Disc(char *bootPath, char *eGSMArgument);

// Boots PS1/PS2 game CD/DVD
int startGameDisc() {
  if (!sceCdInit(SCECdINIT)) {
    bootFail("CDROM ERROR: Failed to initialize libcdvd\n");
    return -ENODEV;
  }

  if (settings.flags & FLAG_USE_DKWDRV)
    DPRINTF("CDROM: Using DKWDRV for PS1 discs\n");
  if (!(settings.flags & FLAG_ENABLE_GAMEID))
    DPRINTF("CDROM: Disabling visual game ID\n");
  if (settings.flags & FLAG_SKIP_PS2_LOGO)
    DPRINTF("CDROM: Skipping PS2LOGO\n");
  if (settings.flags & FLAG_PS1DRV_FAST)
    DPRINTF("CDROM: Forcing PS1DRV fast disc speed\n");
  if (settings.flags & FLAG_PS1DRV_SMOOTH)
    DPRINTF("CDROM: Forcing PS1DRV texture smoothing\n");
  if (settings.flags & FLAG_PS1DRV_USE_VN)
    DPRINTF("CDROM: Running PS1DRV via PS1VModeNeg\n");

  // Wait until the drive is ready
  sceCdDiskReady(0);
  int discType = sceCdGetDiskType();

  if ((discType >= SCECdNODISC) && (discType < SCECdUNKNOWN)) {
    msg("\tWaiting for disc\n");
    while ((discType >= SCECdNODISC) && (discType < SCECdUNKNOWN)) {
      sleep(1);
      discType = sceCdGetDiskType();
    }
  }

  // Make sure the disc is a valid PS1/PS2 disc
  discType = sceCdGetDiskType();
  if (!(discType >= SCECdPSCD || discType <= SCECdPS2DVD)) {
    bootFail("CDROM ERROR: Unsupported disc type\n");
    return -EINVAL;
  }

  // Parse SYSTEM.CNF
  SystemCNFOptions opts = {0};
  discType = parseDiscCNF(&opts);
  if (discType < 0 || (!opts.bootPath && discType != ExecType_PS1)) {
    freeSystemCNFOptions(&opts);
    bootFail("CDROM ERROR: Failed to parse SYSTEM.CNF\n");
    return -ENOENT;
  }

  char *bootPath = NULL;
  char *titleID = NULL;
  char *titleVersion = NULL;
  if (opts.bootPath)
    bootPath = strdup(opts.bootPath);
  if (opts.titleID)
    titleID = strdup(opts.titleID);
  if (opts.titleVersion)
    titleVersion = strdup(opts.titleVersion);
  else
    titleVersion = strdup("???");
  freeSystemCNFOptions(&opts);

  DPRINTF("====\nSYSTEM.CNF:\n");
  if (bootPath)
    DPRINTF("Boot path: %s\n", bootPath);
  if (titleVersion)
    DPRINTF("Title version: %s\n", titleVersion);
  if (titleID)
    DPRINTF("Title ID: %s\n", titleID);
  DPRINTF("====\n");

  if (titleID && titleID[0] != '\0') {
    // Update history file and display game ID
    updateHistoryFile(titleID);
    if (settings.flags & FLAG_ENABLE_GAMEID)
      gsDisplayGameID(titleID);
  } else
    titleID = strdup("???"); // Set placeholder value

  sceCdInit(SCECdEXIT);

  switch (discType) {
  case ExecType_PS1:
    free(bootPath);
    handlePS1Disc(titleID, titleVersion);
    break;
  case ExecType_PS2:
    handlePS2Disc(bootPath, getOSDGSMArgument(titleID));
    break;
  default:
    bootFail("CDROM ERROR: unknown disc type\n");
  }

  return -1;
}

// Boots PS1 disc using PS1DRV or DKWDRV
void handlePS1Disc(char *titleID, char *titleVersion) {
  char **argv = malloc(2 * sizeof(char *));
  if (settings.flags & FLAG_USE_DKWDRV) {
    // Mount the partition and make sure DKWDRV ELF exists
    mountPFS(HOSD_DKWDRV_PATH);
    char *elfPath = (strstr(HOSD_DKWDRV_PATH, ":pfs")) + 1;
    int res = checkFile(elfPath);
    umountPFS();
    // If the ELF exists, boot the disc via DKWDRV
    if (res >= 0) {
      DPRINTF("Starting DKWDRV\n");
      free(titleID);
      free(titleVersion);
      argv[0] = HOSD_DKWDRV_PATH;
      LoadOptions opts = {
          .argc = 1,
          .argv = argv,
      };
      loadELF(&opts);
      return;
    }
    DPRINTF("DKWDRV was not found, falling back to PS1DRV\n");
  }

  // Else, use PS1DRV
  argv[0] = titleID;
  argv[1] = titleVersion;

  // Set PS1DRV flags
  if (settings.flags & (FLAG_PS1DRV_FAST | FLAG_PS1DRV_SMOOTH)) {
    ConfigParam osdConfig;
    GetOsdConfigParam(&osdConfig);
    if (settings.flags & FLAG_PS1DRV_FAST)
      osdConfig.ps1drvConfig |= CDROM_PS1_FAST;
    if (settings.flags & FLAG_PS1DRV_SMOOTH)
      osdConfig.ps1drvConfig |= CDROM_PS1_SMOOTH;
    DPRINTF("Will set PS1DRV config to %x\n", osdConfig.ps1drvConfig);
    SetOsdConfigParam(&osdConfig);
  }

  if (settings.flags & FLAG_PS1DRV_USE_VN) {
    DPRINTF("Starting PS1DRV via PS1VModeNeg with title ID %s and version %s\n", argv[0], argv[1]);
    LoadOptions opts = {
        .elfMem = ps1vn_elf,
        .elfSize = size_ps1vn_elf,
        .argc = 2,
        .argv = argv,
    };
    loadELF(&opts);
    return;
  } else {
    DPRINTF("Starting PS1DRV with title ID %s and version %s\n", argv[0], argv[1]);
    shutdownDEV9();
    sceSifExitCmd();
    LoadExecPS2("rom0:PS1DRV", 2, argv);
  }
}

// Boots PS2 disc directly or via PS2LOGO
void handlePS2Disc(char *bootPath, char *eGSMArgument) {
  char **argv = malloc(2 * sizeof(char *));
  LoadOptions opts = {0};
  if (settings.flags & FLAG_SKIP_PS2_LOGO) {
    argv[0] = bootPath;
    opts.argv = argv;
    opts.argc = 1;
  } else {
    argv[0] = "rom0:PS2LOGO";
    argv[1] = bootPath;
    opts.argv = argv;
    opts.argc = 2;
  }

  if (eGSMArgument) {
    DPRINTF("CDROM: Using eGSM to force the video mode: %s\n", eGSMArgument);
    opts.eGSM = eGSMArgument;
  }

  loadELF(&opts);
}

static char dvdPlayerPath[] = "mcX:/BXEXEC-DVDPLAYER/dvdplayer.elf";

// Reads ROM version from rom0:ROMVER and initializes dvdPlayerPath with region-specific letter
static inline int setDVDPlayerDir(void) {
  int romverFd = open("rom0:ROMVER", O_RDONLY);
  if (romverFd < 0) {
    return -ENOENT;
  }

  char romverStr[5];
  read(romverFd, romverStr, 5);
  close(romverFd);

  switch (romverStr[4]) {
  case 'C': // China
    dvdPlayerPath[6] = 'C';
    break;
  case 'E': // Europe
    dvdPlayerPath[6] = 'E';
    break;
  case 'H': // Asia
  case 'A': // USA
    dvdPlayerPath[6] = 'A';
    break;
  default: // Japan
    dvdPlayerPath[6] = 'I';
  }

  return 0;
}

// Starts the DVD Player from a memory card or ROM
int startDVDVideo() {
  char *argv[4];

  // Check memory cards for DVD Player update first
  if (!setDVDPlayerDir()) {
    int res = -1;

    for (int i = '0'; i < '2'; i++) {
      dvdPlayerPath[2] = i;
      if ((res = checkFile(dvdPlayerPath)) >= 0)
        break;
    }

    if (res >= 0) {
      shutdownDEV9();

      // Build the executable path
      char elfArg[39];
      sprintf(elfArg, "-x %s", dvdPlayerPath);

      // Build arguments for moduleload
      argv[0] = "-m rom0:SIO2MAN";
      argv[1] = "-m rom0:MCMAN";
      argv[2] = "-m rom0:MCSERV";
      argv[3] = elfArg;

      LoadExecPS2("moduleload", 4, argv);
      return -1;
    }
  }

  // Boot the built-in DVD Player from ROM
  if (checkFile("rom1:DVDID") < 0) {
    bootFail("This PlayStation 2 system does not have the built-in DVD Player.\n\tPlease install a DVD Player update to your memory card.");
  }

  shutdownDEV9();

  argv[0] = "-k rom1:EROMDRV";
  argv[1] = "-m erom0:UDFIO";
  argv[2] = "-x erom0:DVDELF";

  LoadExecPS2("moduleload2 rom1:UDNL rom1:DVDCNF", 3, argv);
  return -1;
}

// Executes the sceCdAutoAdjustCtrl call
void cdAutoAdjust(int mode) {
  if (!sceCdInit(SCECdINIT))
    return;

  int res;
  uint32_t result;
  do {
    res = sceCdAutoAdjustCtrl(mode, (u32 *)&result);
  } while ((res == 0) && (result & 0x80) != 0);

  sceCdInit(SCECdEXIT);
}

// Describes the first two OSD NVRAM blocks
typedef struct {
  // PS1DRV settings block
  // Byte 0
  uint8_t ps1drvDiscSpeed : 4;
  uint8_t ps1drvTextureMapping : 4;
  // Bytes 1-14, unused
  uint8_t unused1[14];

  // PlayStation 2 settings block
  // Byte 0
  uint8_t ps2SpdifDisabled : 1;
  uint8_t ps2ScreenType : 2;
  uint8_t ps2VideoOutput : 1;
  uint8_t ps2OldLanguage : 1;
  uint8_t ps2ConfigVersion : 1;
  uint8_t ps2Reserved : 2;
  // Byte 1
  uint8_t ps2NewLanguage : 5;
  uint8_t ps2MaxVersion : 3;
  // Byte 2
  uint8_t ps2TimezoneHigh : 3;
  uint8_t ps2DaylightSavings : 1;
  uint8_t ps2TimeNotation : 1;
  uint8_t ps2DateNotation : 2;
  uint8_t ps2OOBEFlag : 1;
  // Byte 3
  uint8_t ps2TimezoneLow;
  // Bytes 4-14, the rest of config block
  uint8_t unused2[11];
} OSDNVRAMConfig;

// Updates MechaCon NVRAM config with selected PS1DRV options, screen type and language.
// Applies kernel patches and sets the OSD configuration
void initializeOSDConfig() {
  sceCdInit(SCECdINoD);
  int res;
  uint32_t status;

  do {
    res = sceCdOpenConfig(1, 0, 2, (u32 *)&status);
    if (!res)
      return;
  } while (status & 0x81);

  uint8_t buffer[30] = {0};

  do {
    res = sceCdReadConfig(buffer, (u32 *)&status);
  } while ((status & 0x81) || (res == 0));
  do {
    res = sceCdCloseConfig((u32 *)&status);
  } while ((status & 0x81) || (res == 0));

  OSDNVRAMConfig *cnf = (OSDNVRAMConfig *)buffer;
  DPRINTF("NVRAM Settings:\n\tPS1DRV:\n\t\tDisc speed: %d\n\t\tMapping: %d\n\tOSD:\n\t\tSPDIF: %d\n\t\tScreen: %d\n\t\tVideo Out: %d\n\t\tOld "
          "language: %d\n\t\tConfig version: %d\n\t\tLanguage: %d\n\t\tMax version: %d\n\t\tTimezoneH: "
          "%d\n\t\tTimezoneL: %d\n",
          cnf->ps1drvDiscSpeed, cnf->ps1drvTextureMapping, cnf->ps2SpdifDisabled, cnf->ps2ScreenType, cnf->ps2VideoOutput, cnf->ps2OldLanguage,
          cnf->ps2ConfigVersion, cnf->ps2NewLanguage, cnf->ps2MaxVersion, cnf->ps2TimezoneHigh, cnf->ps2TimezoneLow);

  int isUpdated = 0;
  // Set PS1DRV flags
  if ((settings.flags & FLAG_PS1DRV_FAST) && (!cnf->ps1drvDiscSpeed)) {
    DPRINTF("Enabling fast PS1 disc speed\n");
    isUpdated = 1;
    cnf->ps1drvDiscSpeed = 1;
  }
  if ((settings.flags & FLAG_PS1DRV_SMOOTH) && (!cnf->ps1drvTextureMapping)) {
    DPRINTF("Enabling smooth PS1 texture mapping\n");
    isUpdated = 1;
    cnf->ps1drvTextureMapping = 1;
  }

  // Set OSD options
  if ((settings.osdScreenType >= 0) && (settings.osdScreenType != cnf->ps2ScreenType)) {
    DPRINTF("Changing screen type to %d\n", settings.osdScreenType);
    isUpdated = 1;
    cnf->ps2ScreenType = settings.osdScreenType;
  }
  if ((settings.osdLanguage >= LANGUAGE_JAPANESE) && (cnf->ps2NewLanguage != settings.osdLanguage)) {
    DPRINTF("Changing language to %d\n", settings.osdLanguage);
    isUpdated = 1;
    cnf->ps2OldLanguage = (settings.osdLanguage == LANGUAGE_JAPANESE) ? settings.osdLanguage : LANGUAGE_ENGLISH;
    cnf->ps2NewLanguage = settings.osdLanguage;
  }

  if (isUpdated) {
    // Update NVRAM config
    DPRINTF("Updating NVRAM config\n");

    do {
      res = sceCdOpenConfig(1, 1, 2, (u32 *)&status);
      if (!res)
        return;
    } while (status & 0x09);

    do {
      res = sceCdWriteConfig(buffer, (u32 *)&status);
    } while ((status & 0x09) || (res == 0));

    do {
      res = sceCdCloseConfig((u32 *)&status);
    } while ((status & 0x09) || (res == 0));
  }
  sceCdInit(SCECdEXIT);

  // Apply kernel patches for early kernels
  InitOsd();

  // Set kernel config
  ConfigParam osdConfig = {0};
  // Initialize ConfigParam values
  osdConfig.version = 2;
  osdConfig.spdifMode = cnf->ps2SpdifDisabled;
  osdConfig.screenType = cnf->ps2ScreenType;
  osdConfig.videoOutput = cnf->ps2VideoOutput;
  osdConfig.timezoneOffset = ((uint32_t)cnf->ps2TimezoneHigh << 8 | (uint32_t)cnf->ps2TimezoneLow) & 0x7FF;

  if (cnf->ps1drvDiscSpeed)
    osdConfig.ps1drvConfig |= CDROM_PS1_FAST;
  if (cnf->ps1drvTextureMapping)
    osdConfig.ps1drvConfig |= CDROM_PS1_SMOOTH;

  // Force ConfigParam language to English if one of extended languages is used
  osdConfig.language = (cnf->ps2NewLanguage > LANGUAGE_PORTUGUESE) ? LANGUAGE_ENGLISH : cnf->ps2NewLanguage;
  osdConfig.japLanguage = (cnf->ps2OldLanguage == LANGUAGE_JAPANESE) ? cnf->ps2OldLanguage : LANGUAGE_ENGLISH;

  // Set ConfigParam and check whether the version was not set
  // Early kernels can't retain the version value
  SetOsdConfigParam(&osdConfig);
  GetOsdConfigParam(&osdConfig);

  DPRINTF("Kernel Settings:\n\tOSD1:\n\t\tVersion: %d\n\t\tSPDIF: %d\n\t\tScreen Type: %d\n\t\tVideo Output: %d\n\t\tLanguage 1: %d\n\t\tLanguage 2: "
          "%d\n\t\tTZ Offset: "
          "%d\n\t\tPS1DRV: %d\n",
          osdConfig.version, osdConfig.spdifMode, osdConfig.screenType, osdConfig.videoOutput, osdConfig.japLanguage, osdConfig.language,
          osdConfig.timezoneOffset, osdConfig.ps1drvConfig);

  if (osdConfig.version != 0) {
    // This kernel supports ConfigParam2.
    // Initialize ConfigParam2 values
    Config2Param osdConfig2 = {0};
    GetOsdConfigParam2(&osdConfig2, sizeof(osdConfig2), 0);

    osdConfig2.format = 2;
    osdConfig2.version = 2;
    osdConfig2.language = cnf->ps2NewLanguage;
    osdConfig2.timeFormat = cnf->ps2TimeNotation;
    osdConfig2.dateFormat = cnf->ps2DateNotation;
    osdConfig2.daylightSaving = cnf->ps2DaylightSavings;

    SetOsdConfigParam2(&osdConfig2, sizeof(osdConfig2), 0);

    DPRINTF("\tOSD2:\n\t\tVersion: %d\n\t\tFormat: %d\n\t\tDaylight Savings: %d\n\t\tTime Format: %d\n\t\tDate Format: %d\n\t\tLanguage: %d\n",
            osdConfig2.format, osdConfig2.daylightSaving, osdConfig2.timeFormat, osdConfig2.dateFormat, osdConfig2.version, osdConfig2.language);
  }
}
