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

// Attempts to load per-title GSM argument from the GSM_CONF_PATH/HOSDGSM_CONF_PATH depending on the device hint
char *getOSDGSMArgument(char *titleID);
// Works around the CD/DVD loading issue on <70k consoles by cycling the tray
void applyOSDCdQuirk();

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

    if (!strcmp("nologo", arg))
      skipPS2LOGO = 1;
    else if (!strcmp("nogameid", arg))
      displayGameID = 0;
    else if (!strcmp("ps1fast", arg))
      ps1drvFlags |= CDROM_PS1_FAST;
    else if (!strcmp("ps1smooth", arg))
      ps1drvFlags |= CDROM_PS1_SMOOTH;
    else if (!strcmp("ps1vneg", arg))
      ps1drvFlags |= CDROM_PS1_VN;
    else if (!strncmp("dkwdrv", arg, 6)) {
      useDKWDRV = 1;
      dkwdrvPath = strchr(arg, '=');
      if (dkwdrvPath) {
        dkwdrvPath++;
      }
    }
  }

  if (useDKWDRV && !dkwdrvPath)
    dkwdrvPath = strdup(DKWDRV_PATH);

  return startCDROM(displayGameID, skipPS2LOGO, ps1drvFlags, dkwdrvPath, 0);
}

int startCDROM(int displayGameID, int skipPS2LOGO, int ps1drvFlags, char *dkwdrvPath, int skipInit) {
  // Always reset IOP to a known state
  if (!skipInit) {
    int res = 0;
#ifdef APA
    if (settings.deviceHint == Device_PFS)
      res = initPFS(HOSD_CONF_PARTITION, 0, Device_CDROM);
    else
#endif
      res = initModules(Device_CDROM, 0);
    if (res) {
      msg("CDROM ERROR: Failed to initialize modules\n");
      return res;
    }
  }

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
  } else if (settings.flags & FLAG_OSDBOOT) {
    applyOSDCdQuirk();
    sceCdDiskReady(0);
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
    // Apply eGSM arguments when launching PS2 discs
    if (discType == ExecType_PS2) {
      if (!settings.gsmArgument)
        settings.gsmArgument = getOSDGSMArgument(titleID);
      if (settings.gsmArgument)
        DPRINTF("CDROM: Using eGSM to force the video mode: %s\n", settings.gsmArgument);
    }

    // Update history file and display game ID
    settings.flags &= ~(FLAG_APP_GAMEID); // Remove the global flag
    updateHistoryFile(titleID);
    if (displayGameID)
      gsDisplayGameID(titleID);
  } else
    strcpy(titleID, "???"); // Set placeholder value

  if (titleVersion[0] == '\0')
    // Set placeholder value
    strcpy(titleVersion, "???");

  sceCdInit(SCECdEXIT);
  if (settings.deviceHint == Device_PFS)
    deinitPFS();

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

      // Launch PS2 game directly
      char *argv[] = {bootPath};
      LoadELFFromFile(1, argv);
    } else {
      // Launch PS2 game with rom0:PS2LOGO
      char *argv[] = {"rom0:PS2LOGO", bootPath};
      LoadELFFromFile(2, argv);
    }
    break;
  default:
    msg("CDROM ERROR: unknown disc type\n");
  }

  return -1;
}

// Attempts to load per-title GSM argument from the GSM_CONF_PATH/HOSDGSM_CONF_PATH depending on the device hint
char *getOSDGSMArgument(char *titleID) {
  DPRINTF("CDROM: Trying to load the eGSM config file\n");
  FILE *gsmConf = NULL;
#ifdef APA
  if (settings.deviceHint == Device_PFS)
    // Check the internal HDD for the eGSM config file
    gsmConf = fopen(PFS_MOUNTPOINT HOSDGSM_CONF_PATH, "r");
#endif
  if (!gsmConf) {
    // Check both for memory cards for the eGSM config file
    char cnfPath[sizeof(GSM_CONF_PATH) + 1];
    strcpy(cnfPath, GSM_CONF_PATH);
    cnfPath[2] = settings.mcHint + '0';
    gsmConf = fopen(cnfPath, "r");
    if (!gsmConf) {
      cnfPath[2] = (settings.mcHint == 1) ? '0' : '1';
      gsmConf = fopen(cnfPath, "r");
    }
  }
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
      DPRINTF("CDROM: eGSM will use the title-specific config\n");
      titleArg = strdup(valuePtr);
      break;
    }

    if (!strncmp(lineBuffer, "default", 7))
      defaultArg = strdup(valuePtr);
  }

  fclose(gsmConf);

  if (titleArg) {
    // If there's a title-specific argument free the defaultArg and set the defaultArg to titleArg
    if (defaultArg)
      free(defaultArg);

    defaultArg = titleArg;
  }

  return defaultArg;
}

// For some reason, launching OSDSYS from homebrew software via ExecOSD or LoadExecPS2 on <70k consoles
// breaks the CD/DVD loading when the disc is already inserted on boot.
// This works around the issue by cycling the tray
void applyOSDCdQuirk() {
  uint8_t outBuffer[5] = {0};
  uint32_t status;
  if (!sceCdMV(outBuffer, (u32 *)&status) || (status & 0x80)) {
    DPRINTF("CDROM OSD Quirk: Failed to get the MechaCon version\n");
    return;
  }

  int mv = (outBuffer[0] << 8) | outBuffer[1];
  DPRINTF("CDROM OSD Quirk: MechaCon version is %04x", mv);
  if (mv >= 0x0600) {
    DPRINTF("\n");
    return;
  }

  DPRINTF(", cycling the tray\n");
  int res = 0;
  // Open the tray
  do {
    sceCdTrayReq(SCECdTrayOpen, (u32 *)&status);
    res = sceCdStatus();
  } while (!(res & 0x1));

  // Close the tray
  do {
    sceCdTrayReq(SCECdTrayClose, (u32 *)&status);
    res = sceCdStatus();
  } while (res & 0x1);
}
