#include "common.h"
#include "config.h"
#include "crypto.h"
#include "disc.h"
#include "dprintf.h"
#include "handlers.h"
#include "hdd.h"
#include "init.h"
#include <kernel.h>
#include <libpad.h>
#include <ps2sdkapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    if ((padState == PAD_STATE_STABLE) || (padState == PAD_STATE_FINDCTP1))
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
