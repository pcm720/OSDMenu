#include "cdrom.h"
#include "common.h"
#include "defaults.h"
#include "dprintf.h"
#include "game_id.h"
#include "handlers.h"
#include "history.h"
#include "init.h"
#include "loader.h"
#include <ctype.h>
#include <fcntl.h>
#include <kernel.h>
#include <libcdvd.h>
#include <osd_config.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PS1 Video Mode Negator variables
extern uint8_t ps1vn_elf[];
extern int size_ps1vn_elf;

const char *getPS1GenericTitleID();

// Launches the disc while displaying the visual game ID and writing to the history file
int handleCDROM(int argc, char *argv[]) {
  // Parse arguments
  int displayGameID = 1;
  int useDKWDRV = 0;
  int skipPS2LOGO = 0;
  // Nibble 0 — disc speed
  // Nibble 1 — texture smoothing
  int ps1drvFlags = 0;

  char *arg;
  char *dkwdrvPath = NULL;
  for (int i = 0; i < argc; i++) {
    arg = argv[i];
    if ((arg == NULL) || (arg[0] != '-'))
      continue;
    arg++;

    if (!strcmp("nologo", arg)) {
      skipPS2LOGO = 1;
    } else if (!strcmp("nogameid", arg)) {
      displayGameID = 0;
    } else if (!strcmp("ps1fast", arg)) {
      ps1drvFlags |= CDROM_PS1_FAST;
    } else if (!strcmp("ps1smooth", arg)) {
      ps1drvFlags |= CDROM_PS1_SMOOTH;
    } else if (!strcmp("ps1vneg", arg)) {
      ps1drvFlags |= CDROM_PS1_VN;
    } else if (!strncmp("dkwdrv", arg, 6)) {
      useDKWDRV = 1;
      dkwdrvPath = strchr(arg, '=');
      if (dkwdrvPath) {
        dkwdrvPath++;
      }
    }
  }

  if (useDKWDRV && !dkwdrvPath)
    dkwdrvPath = strdup(DKWDRV_PATH);

  return startCDROM(displayGameID, skipPS2LOGO, ps1drvFlags, dkwdrvPath);
}

int startCDROM(int displayGameID, int skipPS2LOGO, int ps1drvFlags, char *dkwdrvPath) {
  // Always reset IOP to a known state
  int res = initModules(Device_MemoryCard, 0);
  if (res)
    return res;

  if (!sceCdInit(SCECdINIT)) {
    msg("CDROM ERROR: Failed to initialize libcdvd\n");
    return -ENODEV;
  }

  if (dkwdrvPath)
    DPRINTF("CDROM: Using DKWDRV for PS1 discs\n");
  if (!displayGameID)
    DPRINTF("CDROM: Disabling visual game ID\n");
  if (skipPS2LOGO)
    DPRINTF("CDROM: Skipping PS2LOGO\n");
  if (ps1drvFlags & CDROM_PS1_FAST)
    DPRINTF("CDROM: Forcing PS1DRV fast disc speed\n");
  if (ps1drvFlags & CDROM_PS1_SMOOTH)
    DPRINTF("CDROM: Forcing PS1DRV texture smoothing\n");
  if (ps1drvFlags & CDROM_PS1_VN)
    DPRINTF("CDROM: Running PS1DRV via PS1VModeNeg\n");

  // Wait until the drive is ready
  sceCdDiskReady(0);
  int discType = sceCdGetDiskType();

  if ((discType >= SCECdNODISC) && (discType < SCECdUNKNOWN)) {
    msg("\n\nWaiting for disc\n");
    while ((discType >= SCECdNODISC) && (discType < SCECdUNKNOWN)) {
      sleep(1);
      discType = sceCdGetDiskType();
    }
  }

  // Make sure the disc is a valid PS1/PS2 disc
  discType = sceCdGetDiskType();
  if (!(discType >= SCECdPSCD || discType <= SCECdPS2DVD)) {
    msg("CDROM ERROR: Unsupported disc type\n");
    return -EINVAL;
  }

  // Parse SYSTEM.CNF
  char *bootPath = calloc(sizeof(char), CNF_MAX_STR);
  char *titleID = calloc(sizeof(char), 12);
  char *titleVersion = calloc(sizeof(char), CNF_MAX_STR);
  discType = parseDiscCNF(bootPath, titleID, titleVersion);
  if (discType < 0) {
    msg("CDROM ERROR: Failed to parse SYSTEM.CNF\n");
    free(bootPath);
    free(titleID);
    free(titleVersion);
    return -ENOENT;
  }

  if (titleID[0] != '\0') {
    // Update history file and display game ID
    updateHistoryFile(titleID);
    if (displayGameID)
      gsDisplayGameID(titleID);
  } else
    strcpy(titleID, "???"); // Set placeholder value

  if (titleVersion[0] == '\0')
    // Set placeholder value
    strcpy(titleVersion, "???");

  sceCdInit(SCECdEXIT);

  switch (discType) {
  case ExecType_PS1:
    if (dkwdrvPath) {
      DPRINTF("Starting DKWDRV\n");
      free(bootPath);
      free(titleID);
      free(titleVersion);
      char *argv[] = {dkwdrvPath};
      launchPath(1, argv);
    } else {
      char *argv[] = {titleID, titleVersion};

      // Set PS1DRV flags
      if (ps1drvFlags & (CDROM_PS1_FAST | CDROM_PS1_SMOOTH)) {
        ConfigParam osdConfig;
        GetOsdConfigParam(&osdConfig);
        osdConfig.ps1drvConfig |= (ps1drvFlags & ~CDROM_PS1_VN);
        DPRINTF("Will set PS1DRV config to %x\n", osdConfig.ps1drvConfig);
        SetOsdConfigParam(&osdConfig);
      }

      if (ps1drvFlags & CDROM_PS1_VN) {
        DPRINTF("Starting PS1DRV via PS1VModeNeg with title ID %s and version %s\n", argv[0], argv[1]);
        sceSifExitCmd();
        LoadEmbeddedELF(ps1vn_elf, 2, argv);
      } else {
        char *argv[] = {titleID, titleVersion};
        DPRINTF("Starting PS1DRV with title ID %s and version %s\n", argv[0], argv[1]);
        sceSifExitCmd();
        LoadExecPS2("rom0:PS1DRV", 2, argv);
      }
    }
    break;
  case ExecType_PS2:
    if (skipPS2LOGO) {
      // Apply IOP emulation flags for Deckard consoles
      // (simply returns on PGIF consoles)
      if (titleID[0] != '\0')
        applyXPARAM(titleID);

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
    msg("CDROM ERROR: unknown disc type\n");
  }

  return -1;
}
