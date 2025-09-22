#include "common.h"
#include "config.h"
#include "disc.h"
#include "dprintf.h"
#include "hdd.h"
#include "init.h"
#include "loader.h"
#include "mbrboot.h"
#include "mbrboot_crypto.h"
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
// Handles arguments supported by the PSBBN MBR
int handlePSBBNArgs(int argc, char *argv[]);

int main(int argc, char *argv[]) {
  // Initialize IOP modules
  int res = 0;
  if ((res = initModules())) {
    bootFail("Failed to initialize IOP modules");
    return res;
  }

  if (!strcmp(argv[0], "rom0:MBRBOOT")) {
    DPRINTF("Decrypting PSBBN MBR arguments\n");
    argv = decryptMBRBOOTArgs(&argc, argv);
  }

  // Check if filesystems need checking
  if ((argc > 1) && strcmp(argv[1], "SkipFsck")) {
    switch ((res = isFsckRequired())) {
    case -1:
      // Fail to OSDSYS
      bootFail("Failed to check the hard drive for errors");
      break;
    case 1:
      // Run fsck
      runFsck();
      // Fail to OSDSYS
      bootFail("Failed to launch the fsck utility to check the hard drive for errors");
      break;
    }
  }

  if ((res = loadConfig()))
    printf("WARN: Failed to load the config file: %d, will use defaults\n", res);

  DPRINTF("flags: %x\n", settings.flags);
  launchPath *lpath = settings.paths;
  linkedStr *lstr;
  while (lpath != NULL) {
    lstr = lpath->paths;
    while (lstr != NULL) {
      DPRINTF("trigger %d: path %s\n", lpath->trigger, lstr->str);
      lstr = lstr->next;
    }
    lstr = lpath->args;
    while (lstr != NULL) {
      DPRINTF("trigger %d: arg %s\n", lpath->trigger, lstr->str);
      lstr = lstr->next;
    }
    lpath = lpath->next;
  }

  if (!strcmp(argv[0], "rom0:MBRBOOT"))
    handlePSBBNArgs(argc, argv);

  switch (readPad()) {
  case TRIGGER_TYPE_START:
    printf("Start\n");
    break;
  case TRIGGER_TYPE_SQUARE:
    printf("Square\n");
    break;
  case TRIGGER_TYPE_CROSS:
    printf("Cross\n");
    break;
  case TRIGGER_TYPE_CIRCLE:
    printf("Circle\n");
    break;
  case TRIGGER_TYPE_TRIANGLE:
    printf("Triangle\n");
    break;
  default:
    printf("Default\n");
    break;
  }

  // #ifdef HOSDMENU
  //   char **nargv = malloc(2 * sizeof(char *));
  //   nargv[0] = "hosdmenu";
  //   nargv[1] = "-mbrboot";
  //   LoadEmbeddedELF(0, (uint8_t *)payload_elf, 2, nargv);
  // #else
  //   LoadEmbeddedELF(0, (uint8_t *)payload_elf, 0, NULL);
  // #endif

  int ret = 0x20000000;
  while (ret--)
    asm("nop\nnop\nnop\nnop");
  __builtin_trap();
}

// Initiailizes the pad library and returns the button pressed
TriggerType readPad() {
  struct padButtonStatus buttons;
  static char padBuf[256] __attribute__((aligned(64)));

  padInit(0);
  padPortOpen(0, 0, padBuf);
  int padState = 0;
  while ((padState = padGetState(0, 0))) {
    if (padState == PAD_STATE_STABLE)
      break;
    if (padState == PAD_STATE_DISCONN)
      return TRIGGER_TYPE_AUTO;
  }

  int retries = 10;
  while (retries--) {
    if (padRead(0, 0, &buttons) != 0) {
      uint32_t paddata = 0xffff ^ buttons.btns;
      if (paddata & PAD_START)
        return TRIGGER_TYPE_START;
      if (paddata & PAD_TRIANGLE)
        return TRIGGER_TYPE_TRIANGLE;
      if (paddata & PAD_CIRCLE)
        return TRIGGER_TYPE_CIRCLE;
      if (paddata & PAD_CROSS)
        return TRIGGER_TYPE_CROSS;
      if (paddata & PAD_SQUARE)
        return TRIGGER_TYPE_SQUARE;
    }
  }
  padEnd();
  return TRIGGER_TYPE_AUTO;
}

// Handles arguments supported by the PSBBN MBR
int handlePSBBNArgs(int argc, char *argv[]) {
  if (!strcmp(argv[1], "BootError")) {
    char *nargv[1] = {"BootError"};
    ExecOSD(1, nargv);
  } else if (!strcmp(argv[1], "BootClock")) {
    char *nargv[1] = {"BootClock"};
    ExecOSD(1, nargv);
  } else if (!strcmp(argv[1], "BootBrowser")) {
    char *nargv[1] = {"BootBrowser"};
    ExecOSD(1, nargv);
  } else if (!strcmp(argv[1], "BootCdPlayer")) {
    char *nargv[1] = {"BootCdPlayer"};
    ExecOSD(1, nargv);
  } else if (!strcmp(argv[1], "BootOpening")) {
    char *nargv[1] = {"BootOpening"};
    ExecOSD(1, nargv);
  } else if (!strcmp(argv[1], "BootWarning")) {
    char *nargv[1] = {"BootWarning"};
    ExecOSD(1, nargv);
  } else if (!strcmp(argv[1], "BootIllegal")) {
    char *nargv[1] = {"BootIllegal"};
    ExecOSD(1, nargv);
  } else if (!strcmp(argv[1], "Initialize")) {
    char *nargv[1] = {"Initialize"};
    ExecOSD(1, nargv);
  } else if ((!strcmp(argv[1], "BootPs1Cd")) || (!strcmp(argv[1], "BootPs2Cd")) || (!strcmp(argv[1], "BootPs2Dvd")))
    return startGameDisc();
  else if (!strcmp(argv[1], "BootDvdVideo"))
    return startDVDVideo();
  else if (!strcmp(argv[1], "BootHddApp")) {
    return startHDDApplication(argc, argv);
  } else if (!strcmp(argv[1], "DnasPs1Emu"))
    startDNAS(argc, argv);
  else if (!strcmp(argv[1], "DnasPs2Native"))
    startDNAS(argc, argv);
  else if (!strcmp(argv[1], "DnasPs2Hdd"))
    startDNAS(argc, argv);

  return -1;
}
