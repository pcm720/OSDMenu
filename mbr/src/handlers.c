#include "handlers.h"
#include "cnf.h"
#include "common.h"
#include "config.h"
#include "defaults.h"
#include "disc.h"
#include "dprintf.h"
#include "game_id.h"
#include "hdd.h"
#include "loader.h"
#include "patinfo.h"
#include <ctype.h>
#include <debug.h>
#include <kernel.h>
#include <libpad.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Handles the PSBBN Opt00000000 argument.
// It seems this argument is only used to pass the mode option to the sceCdAutoAdjustCtrl call.
void handleOpt(char *arg) {
  int mode = -1;
  int res = sscanf(arg, "Opt%x", &mode);
  if (res == 1) {
    mode = mode >> 8 & 0x7;
    switch (mode) {
    case 7:
      mode = 3;
      break;
    case 6:
      mode = 2;
      break;
    case 5:
      mode = 1;
      break;
    case 4:
      mode = 0;
      break;
    default:
      mode = -1;
    }
  }

  if (mode != -1) {
    DPRINTF("Calling the sceCdAutoAdjustCtrl with mode %d\n", mode);
    cdAutoAdjust(mode);
  }
}

// Handles arguments supported by the HDD-OSD/PSBBN MBR
int handleOSDArgs(int argc, char *argv[]) {
  int res = 0;

  if (!strncmp(argv[1], "Opt", 3)) {
    // PSBBN may pass the "Opt%x" argument as argv[1], with the subsequent arguments containing the actual args.
    handleOpt(argv[1]);
    argc--;
    argv[1] = argv[0];
    argv = &argv[1];
  }

  if (!strcmp(argv[1], "BootError") || !strcmp(argv[1], "BootClock") || !strcmp(argv[1], "BootBrowser") || !strcmp(argv[1], "BootCdPlayer") ||
      !strcmp(argv[1], "BootOpening") || !strcmp(argv[1], "BootWarning") || !strcmp(argv[1], "BootIllegal") || !strcmp(argv[1], "Initialize") ||
      !strncmp(argv[1], "Skip", 4)) {
    // Execute OSD
    argc--;
    argv = &argv[1];
    execOSD(argc, argv);
  } else if ((!strcmp(argv[1], "BootPs1Cd")) || (!strcmp(argv[1], "BootPs2Cd")) || (!strcmp(argv[1], "BootPs2Dvd")))
    res = startGameDisc();
  else if (!strcmp(argv[1], "BootDvdVideo"))
    res = startDVDVideo();
  else if (!strcmp(argv[1], "BootHddApp")) {
    // Adjust argv to start from the partition path
    int nargc = argc - 2;
    char **nargv = &argv[2];
    res = handleHDDApplication(nargc, nargv);
  } else if (!strcmp(argv[1], "DnasPs1Emu"))
    handleDNAS(argc, argv);
  else if (!strcmp(argv[1], "DnasPs2Native"))
    handleDNAS(argc, argv);
  else if (!strcmp(argv[1], "DnasPs2Hdd"))
    handleDNAS(argc, argv);

  msg("Failed to execute %s: %d\nAdditional information:\n", argv[1], res);
  for (int i = 0; i < argc; i++)
    msg("\targv[%i] = %s\n", i, argv[i]);
  argc--;
  argv[1] = "BootError";
  bootFailWithArgs("Failed to handle the argument\n", argc, &argv[1]);
  return -1;
}

// Starts the dnasload applcation
void handleDNAS(int argc, char *argv[]) {
  // Update the history file
  char *titleID = generateTitleID(argv[2]);
  if (titleID)
    updateLaunchHistory(titleID, 0);

  // Override argv[1] with the dnasload path and adjust argc
  argv[1] = "hdd0:__system:pfs:/dnas100/dnasload.elf";
  argc -= 1;
  LoadOptions opts = {
      .argc = argc,
      .argv = &argv[1],
  };
  loadELF(&opts);
}

// Starts the executable pointed to by argv[0]
// Supports the following paths:
// $HOSDSYS — will run HOSDMenu or HDD-OSD. Make sure argv has space for the additional -mbrboot argument.
// $PSBBN — will run PSBBN without encrypting the arguments
// hdd0:<partition path>:PATINFO — will boot from HDD partition attribute area
// hdd0:<partition path>:pfs:<PFS path> — ELF from the HDD
// mc?: — ELF from the memory card
// cdrom — CD/DVD
// dvd — DVD Player
int handleConfigPath(int argc, char *argv[]) {
  if (!strcmp(argv[argc - 1], "-noflags")) {
    DPRINTF("Resetting configuration flags\n");
    // Reset config flags and remove the last argument
    settings.flags = 0;
    argc--;
  }

  if (!strcmp(argv[0], "cdrom"))
    // Start the CD/DVD
    return startGameDisc();

  if (!strcmp(argv[0], "dvd"))
    // Start the DVD Player
    return startDVDVideo();

  if (!strcmp(argv[0], "$HOSDSYS")) {
    // Start HOSDMenu or HOSDSYS
    if (mountPFS(SYSTEM_PARTITION))
      return -ENODEV;

    return startHOSDSYS(argc, argv);
  }

  char *titleID = NULL;
  if (!strcmp(argv[0], "$PSBBN")) {
    // Start PSBBN osdboot.elf
    if (mountPFS(SYSTEM_PARTITION))
      return -ENODEV;

    if (checkFile("pfs0:" OSDBOOT_PFS_PATH) < 0) {
      umountPFS();
      return -ENOENT;
    }

    umountPFS();

    argv[0] = SYSTEM_PARTITION ":pfs:" OSDBOOT_PFS_PATH;

    if (settings.flags & (FLAG_ENABLE_GAMEID | FLAG_APP_GAMEID))
      titleID = strdup("SCPN_601.60");

    goto start;
  }

  if (!strncmp(argv[0], "mc", 2)) {
    // Run executable from the memory card
    if (checkMCPath(argv[0]))
      return -EINVAL;

    goto start;
  }

  if (!strncmp(argv[0], "hdd0:", 5)) {
    // Run executable from the HDD
    if (strstr(argv[0], "PATINFO"))
      // Handle PATINFO
      return handleHDDApplication(argc, argv);

    // Handle PFS path
    if (checkPFSPath(argv[0]))
      return -ENOENT;

    goto start;
  }

  if (!strncmp(argv[0], "ata:", 4)) {
    // Replace ata: with mass0:
    char *nargv = malloc(strlen(argv[0]) + 7);
    sprintf(nargv, "mass0%s", &argv[0][3]);
    argv[0] = nargv;
  }

  // Fallback to just checking if file exists and attempting to execute it
  if (checkFile(argv[0]) < 0)
    return -ENOENT;

start:
  if (settings.flags & (FLAG_ENABLE_GAMEID | FLAG_APP_GAMEID)) {
    if (!titleID)
      titleID = generateTitleID(argv[0]);
    if (titleID) {
      DPRINTF("Title ID is %s\n", titleID);
      gsDisplayGameID(titleID);
      free(titleID);
    }
  }

  LoadOptions opts = {0};
  opts.argc = argc;
  opts.argv = argv;
  return loadELF(&opts);
}

// Starts HDD application using data from the partition attribute area header
// Assumes argv[0] is the partition path
int handleHDDApplication(int argc, char *argv[]) {
  char *titleID = NULL;
  LoadOptions *lopts = parsePATINFO(argc, argv, &titleID);
  if (!lopts)
    return -ENOENT;

  // Check memory card path
  if (!strncmp(lopts->argv[0], "mc", 2) && (checkMCPath(lopts->argv[0])))
    return -ENOENT;

  // Check PFS path
  if (strstr(lopts->argv[0], ":pfs:") && checkPFSPath(lopts->argv[0]))
    return -ENOENT;

  if (!strncmp(lopts->argv[0], "ata:", 4)) {
    // Replace ata: with mass0: and check if target ELF exists
    char *nargv = malloc(strlen(lopts->argv[0]) + 7);
    sprintf(nargv, "mass0%s", &lopts->argv[0][3]);
    free(lopts->argv[0]);
    lopts->argv[0] = nargv;

    if (checkFile(lopts->argv[0]) < 0)
      return -ENOENT;
  }

  if (titleID) {
    updateLaunchHistory(titleID, (settings.flags & FLAG_APP_GAMEID));
  }

  if (!strcmp(lopts->argv[lopts->argc - 1], "-patinfo"))
    lopts->argc--; // Remove launcher argument from target ELF args

  if (!strncmp(lopts->argv[0], "cdrom", 5)) {
    char *gsmArg = getOSDGSMArgument(titleID);
    free(titleID);
    handlePS2Disc(lopts->argv[0], gsmArg);
    return -1;
  }
  free(titleID);

  return loadELF(lopts);
}
