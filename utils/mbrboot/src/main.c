#include "config.h"
#include "hdd.h"
#include "init.h"
#include "loader.h"
#include "mbrboot.h"
#include "mbrboot_crypto.h"
#include <kernel.h>
#include <libpad.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <debug.h>

// Initiailizes the pad library and returns the button pressed
TriggerType readPad();
// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail();

int main(int argc, char *argv[]) {
  // Initialize IOP modules
  int res = 0;
  if ((res = initModules())) {
    return res;
  }

  // Check if filesystems need checking
  switch ((res = isFsckRequired())) {
  case -1:
    // Fail to OSDSYS
    bootFail();
    break;
  case 1:
    // Run fsck
    runFsck();
    // Fail to OSDSYS
    bootFail();
    break;
  }

  init_scr();
  scr_setCursor(0);
  scr_clear();
  scr_printf("\n\n\n\nargc: %d\n", argc);

  for (int i = 0; i < argc; i++)
    scr_printf("argv[%d]: %s\n", i, argv[i]);

  if (!strcmp(argv[0], "rom0:MBRBOOT")) {
    scr_printf("\nDecrypting arguments\n");
    argv = decryptMBRBOOTArgs(&argc, argv);

    handlePSBBNArgs(argc, argv);

    __builtin_trap();

    int ret = 0x20000000;
    while (ret--)
      asm("nop\nnop\nnop\nnop");
  }

  if (loadConfig())
    bootFail();

  switch (readPad()) {
  case TRIGGER_TYPE_START:
    scr_printf("Start\n");
    break;
  case TRIGGER_TYPE_SQUARE:
    scr_printf("Square\n");
    break;
  case TRIGGER_TYPE_CROSS:
    scr_printf("Cross\n");
    break;
  case TRIGGER_TYPE_CIRCLE:
    scr_printf("Circle\n");
    break;
  case TRIGGER_TYPE_TRIANGLE:
    scr_printf("Triangle\n");
    break;
  default:
  }
}

// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail() {
  shutdownDEV9();

  // Exec OSD with the "BootError" argument
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
