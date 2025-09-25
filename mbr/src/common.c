#include "hdd.h"
#include <debug.h>
#include <fcntl.h>
#include <kernel.h>
#include <ps2sdkapi.h>

static int isScreenInited = 0;

// Returns >=0 if file exists
int checkFile(char *path) {
  int res = open(path, O_RDONLY);
  if (res < 0)
    return res;

  close(res);
  return res;
}

void initScreen() {
  if (isScreenInited)
    return;

  init_scr();
  scr_setCursor(0);
  scr_printf(".\n\n\n\n"); // To avoid messages being hidden by overscan
  isScreenInited = 1;
}

// Displays the message on the screen
void msg(const char *str, ...) {
  va_list args;
  va_start(args, str);

  initScreen();

  scr_vprintf(str, args);

  va_end(args);
}

// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail(char *msg) {
  // Display the error message
  initScreen();
  scr_printf("\t\nFatal error:\n\t%s\n", msg);

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
