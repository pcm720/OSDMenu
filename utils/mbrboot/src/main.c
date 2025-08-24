#include "config.h"
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
// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail(char *msg);

int main(int argc, char *argv[]) {
  // Initialize IOP modules
  int res = 0;
  if ((res = initModules())) {
    bootFail("Failed to initialize IOP modules");
    return res;
  }

  // Check if filesystems need checking
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

  printf("argc: %d\n", argc);

  for (int i = 0; i < argc; i++)
    printf("argv[%d]: %s\n", i, argv[i]);

  if (!strcmp(argv[0], "rom0:MBRBOOT")) {
    scr_printf("\nDecrypting arguments\n");
    argv = decryptMBRBOOTArgs(&argc, argv);

    handlePSBBNArgs(argc, argv);

    __builtin_trap();
  }

  if ((res = loadConfig()))
    printf("WARN: Failed to load the config file: %d, will use defaults\n", res);

  printf("flags: %x\n", settings.flags);
  launchPath *lpath = settings.paths;
  linkedStr *lstr;
  while (lpath != NULL) {
    lstr = lpath->paths;
    while (lstr != NULL) {
      printf("trigger %d: path %s\n", lpath->trigger, lstr->str);
      lstr = lstr->next;
    }
    lstr = lpath->args;
    while (lstr != NULL) {
      printf("trigger %d: arg %s\n", lpath->trigger, lstr->str);
      lstr = lstr->next;
    }
    lpath = lpath->next;
  }

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
  }

  int ret = 0x20000000;
  while (ret--)
    asm("nop\nnop\nnop\nnop");
  __builtin_trap();
}

// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail(char *msg) {
  // Display the error message
  init_scr();
  scr_setCursor(0);
  scr_clear();
  scr_printf(".\n\n\n\n\tFatal error:\n\t%s\n", msg);

  // Shutdown dev9
  shutdownDEV9();

  // Delay
  int ret = 0x20000000;
  while (ret--)
    asm("nop\nnop\nnop\nnop");

  // Execute OSDSYS with the "BootError" argument
  char *args[] = {"BootError"};
  ExecOSD(1, args);
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
