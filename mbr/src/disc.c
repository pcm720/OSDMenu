#include "cdrom.h"
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
// Attempts to load per-title GSM argument from the GSM_CONF_PATH/HOSDGSM_CONF_PATH depending on the device hint
char *getOSDGSMArgument(char *titleID);
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
  if (settings.flags & FLAG_DISABLE_GAMEID)
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
  char *bootPath = calloc(sizeof(char), CNF_MAX_STR);
  char *titleID = calloc(sizeof(char), 12);
  char *titleVersion = calloc(sizeof(char), CNF_MAX_STR);
  discType = parseDiscCNF(bootPath, titleID, titleVersion);
  if (discType < 0) {
    bootFail("CDROM ERROR: Failed to parse SYSTEM.CNF\n");
    free(bootPath);
    free(titleID);
    free(titleVersion);
    return -ENOENT;
  }

  DPRINTF("====\nSYSTEM.CNF:\nBoot path: %s\nTitle ID: %s\nTitle version: %s\n====\n", bootPath, titleID, titleVersion);

  if (titleID[0] != '\0') {
    // Update history file and display game ID
    updateHistoryFile(titleID);
    if (!(settings.flags & FLAG_DISABLE_GAMEID))
      gsDisplayGameID(titleID);
  } else
    strcpy(titleID, "???"); // Set placeholder value

  if (titleVersion[0] == '\0')
    // Set placeholder value
    strcpy(titleVersion, "???");

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
  shutdownDEV9();
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
    sceSifExitCmd();
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
    sceSifExitCmd();
    LoadExecPS2("rom0:PS1DRV", 2, argv);
  }
}

// Attempts to load per-title GSM argument from the GSM_CONF_PATH/HOSDGSM_CONF_PATH depending on the device hint
char *getOSDGSMArgument(char *titleID) {
  DPRINTF("Trying to load the eGSM config file\n");
  if (mountPFS(HOSD_CONF_PARTITION))
    return NULL;

  FILE *gsmConf = fopen("pfs0:" HOSDGSM_CONF_PATH, "r");
  if (!gsmConf)
    return NULL;

  char *defaultArg = NULL;
  char *titleArg = NULL;
  char lineBuffer[30] = {0};
  char *valuePtr = NULL;
  while (fgets(lineBuffer, sizeof(lineBuffer), gsmConf)) { // fgets returns NULL if EOF or an error occurs
    // Find the start of the value
    valuePtr = strchr(lineBuffer, '=');
    if (!valuePtr)
      continue;
    *valuePtr = '\0';

    // Trim whitespace and terminate the value
    do {
      valuePtr++;
    } while (isspace((int)*valuePtr));
    valuePtr[strcspn(valuePtr, "\r\n")] = '\0';

    if (!strncmp(lineBuffer, titleID, 11)) {
      DPRINTF("eGSM will use the title-specific config\n");
      titleArg = strdup(valuePtr);
      break;
    }

    if (!strncmp(lineBuffer, "default", 7))
      defaultArg = strdup(valuePtr);
  }

  fclose(gsmConf);
  umountPFS();

  if (titleArg) {
    // If there's a title-specific argument free the defaultArg and set the defaultArg to titleArg
    if (defaultArg)
      free(defaultArg);

    defaultArg = titleArg;
  }

  return defaultArg;
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
