#include "cnf.h"
#include "common.h"
#include "config.h"
#include "crypto.h"
#include "defaults.h"
#include "disc.h"
#include "dprintf.h"
#include "game_id.h"
#include "hdd.h"
#include "init.h"
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

// Embedded payload ELF
extern uint8_t payload_elf[];
extern int size_payload_elf;

// Placeholder entrypoint function for the PS2SDK crt0, to be placed at 0x100000.
// Jumps to the actual entrypoint (__start)
__attribute__((weak)) void __start();
__attribute__((noreturn)) void __entrypoint() {
  asm volatile("# Jump to crt0 __start \n"
               "j      %0              \n"
               :
               : "Csy"(__start)
               :);
  __builtin_unreachable();
}

// Initiailizes the pad library and returns the button pressed
TriggerType readPad();

// Handles arguments supported by the HDD-OSD/PSBBN MBR
int handleOSDArgs(int argc, char *argv[]);

// Starts the executable pointed to by argv[0]
int handleConfigPath(int argc, char *argv[]);

// Starts the dnasload applcation
void startDNAS(int argc, char *argv[]);

// Starts HDD application using data from the partition attribute area header
// Assumes argv[0] is the partition path
int startHDDApplication(int argc, char *argv[]);

int main(int argc, char *argv[]) {
  // Initialize IOP modules
  int res = 0;
  if ((res = initModules())) {
    fatalMsg("Failed to initialize IOP modules");
    char *args[] = {"BootError"};
    ExecOSD(1, args);
    return res;
  }

  if (argc > 1) {
    if (!strcmp(argv[0], "rom0:MBRBOOT")) {
      DPRINTF("Decrypting PSBBN MBR arguments\n");
      argv = decryptMBRBOOTArgs(&argc, argv);
    }

    // Check if filesystems need checking
    if (strcmp(argv[1], "SkipFsck")) {
      res = isFsckRequired();
      if (res != 0) {
        if (res < 0)
          fatalMsg("Failed to launch the fsck utility to check the hard drive for errors");
        else {
          // Run fsck
          runFsck();
          // Fail to OSD
          fatalMsg("Failed to launch the fsck utility to check the hard drive for errors");
        }

        // Fail to OSDSYS
        shutdownDEV9();
        char *args[] = {"BootError"};
        ExecOSD(1, args);
      }
    }
  }

  if ((res = loadConfig()))
    DPRINTF("WARN: Failed to load the config file: %d, will use defaults\n", res);

  updateOSDSettings();

  // Handle OSD arguments when there are any additional arguments when running as rom0:MBRBOOT or rom0:HDDBOOT
  if ((argc > 1) && (!strcmp(argv[0], "rom0:MBRBOOT") || !strcmp(argv[0], "rom0:HDDBOOT")) && (strlen(argv[1]) > 0))
    return handleOSDArgs(argc, argv);

  // Else, read controller inputs
  TriggerType trigger = readPad();

  // Handle paths
  launchPath *lpath = settings.paths;
  linkedStr *lstr;
  int argIdx = 0;
  while (lpath != NULL) {
    if (lpath->trigger != trigger) {
      lpath = lpath->next;
      continue;
    }
    lstr = lpath->paths;

    DPRINTF("Found an entry for trigger %d: argc is %d\n", lpath->trigger, lpath->argCount);

    // Assemble argv
    char **args = malloc(lpath->argCount + 2); // Reserve two more for argv[0] and possible HOSDMenu -mbrboot flag
    if (lpath->argCount > 0) {
      linkedStr *arg = lpath->args;
      argIdx = 1;
      while (arg != NULL) {
        DPRINTF("argv[%d]: %s\n", argIdx, arg->str);
        args[argIdx++] = arg->str;
        arg = arg->next;
      }
    }

    // Try all defined paths
    while (lstr != NULL) {
      DPRINTF("Trying path %s\n", lstr->str);
      args[0] = lstr->str;

      argIdx = handleConfigPath(lpath->argCount + 1, args);
      DPRINTF("Failed to start: %d\n", argIdx);

      lstr = lstr->next;
    }

    free(args);
    lpath = lpath->next;
  }

  // Fallback to loading HOSDMenu/HOSDSYS/PSBBN, skipping the HDD Update check to prevent boot loops
  DPRINTF("All paths were tried, falling back to OSD\n");
  char *args[] = {"SkipHdd"};
  execOSD(1, args);
}

// Initiailizes the pad library and returns the button pressed
TriggerType readPad() {
  static char padBuf[256] __attribute__((aligned(64)));
  TriggerType t = TRIGGER_TYPE_AUTO;
  struct padButtonStatus buttons;
  uint32_t paddata = 0;

  if (padInit(0) != 1) {
    DPRINTF("Failed to init libpad\n");
    return t;
  }

  int retries = 10;
  while (retries--) {
    if (padPortOpen(0, 0, padBuf) != 0)
      goto next;
  }
  DPRINTF("Failed to open the pad port\n");
  padEnd();
  return t;

next:
  int padState = 0;
  while ((padState = padGetState(0, 0))) {
    if (padState == PAD_STATE_STABLE)
      break;
    if (padState == PAD_STATE_DISCONN) {
      DPRINTF("Pad is disconnected\n");
      padPortClose(0, 0);
      padEnd();
      return t;
    }
  }

  retries = 10;
  while (retries--) {
    if (padRead(0, 0, &buttons) != 0) {
      paddata = (0xffff ^ buttons.btns) & ~paddata;
      if (paddata & PAD_START) {
        t = TRIGGER_TYPE_START;
        break;
      }
      if (paddata & PAD_TRIANGLE) {
        t = TRIGGER_TYPE_TRIANGLE;
        break;
      }
      if (paddata & PAD_CIRCLE) {
        t = TRIGGER_TYPE_CIRCLE;
        break;
      }
      if (paddata & PAD_CROSS) {
        t = TRIGGER_TYPE_CROSS;
        break;
      }
      if (paddata & PAD_SQUARE) {
        t = TRIGGER_TYPE_SQUARE;
        break;
      }
    }
  }
  padPortClose(0, 0);
  padEnd();
  return t;
}

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
    res = startHDDApplication(nargc, nargv);
  } else if (!strcmp(argv[1], "DnasPs1Emu"))
    startDNAS(argc, argv);
  else if (!strcmp(argv[1], "DnasPs2Native"))
    startDNAS(argc, argv);
  else if (!strcmp(argv[1], "DnasPs2Hdd"))
    startDNAS(argc, argv);

  msg("Failed to execute %s: %d\nAdditional information:\n", argv[1], res);
  for (int i = 0; i < argc; i++)
    msg("\targv[%i] = %s\n", i, argv[i]);
  argc--;
  argv[1] = "BootError";
  bootFailWithArgs("Failed to handle the argument\n", argc, &argv[1]);
  return -1;
}

// Starts the dnasload applcation
void startDNAS(int argc, char *argv[]) {
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
      return startHDDApplication(argc, argv);

    // Handle PFS path
    if (checkPFSPath(argv[0]))
      return -ENOENT;

    goto start;
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

  LoadOptions opts = {};
  if (argc > 1 && !strncmp(argv[argc - 1], "-gsm=", 5)) {
    opts.eGSM = strdup(&argv[argc - 1][5]);
    argc--;
  }

  opts.argc = argc;
  opts.argv = argv;
  return loadELF(&opts);
}

// Starts HDD application using data from the partition attribute area header
// Assumes argv[0] is the partition path
int startHDDApplication(int argc, char *argv[]) {
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
