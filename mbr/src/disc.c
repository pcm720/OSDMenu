#include "cdrom.h"
#include "common.h"
#include "config.h"
#include "defaults.h"
#include "dprintf.h"
#include "game_id.h"
#include "hdd.h"
#include "history.h"
#include "loader.h"
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
    shutdownDEV9();
    if (settings.flags & FLAG_SKIP_PS2_LOGO) {
      sceSifExitCmd();
      // Launch PS2 game directly
      LoadExecPS2(bootPath, 0, NULL);
    } else {
      sceSifExitCmd();
      // Launch PS2 game with rom0:PS2LOGO
      char *argv[] = {bootPath};
      LoadExecPS2("rom0:PS2LOGO", 1, argv);
    }
    break;
  default:
    bootFail("CDROM ERROR: unknown disc type\n");
  }

  return -1;
}

// Boots PS1 disc using PS1DRV or DKWDRV
void handlePS1Disc(char *titleID, char *titleVersion) {
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
      char *argv[] = {HOSD_DKWDRV_PATH};
      LoadELFFromFile(0, 1, argv);
    }
    DPRINTF("DKWDRV was not found, falling back to PS1DRV\n");
  }

  // Else, use PS1DRV
  shutdownDEV9();
  char *argv[] = {titleID, titleVersion};

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
    LoadEmbeddedELF(0, ps1vn_elf, 2, argv);
  } else {
    char *argv[] = {titleID, titleVersion};
    DPRINTF("Starting PS1DRV with title ID %s and version %s\n", argv[0], argv[1]);
    sceSifExitCmd();
    LoadExecPS2("rom0:PS1DRV", 2, argv);
  }
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
