#include "osdr.h"
#include "psxinit.h"
#include "settings.h"
#include "splash.h"
#include <loadfile.h>
#include <ps2sdkapi.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>
#include <io_common.h>

// On PSX, switches PSX into PS2 mode, loads XFROM modules, OSDR and config files and returns 1
int initPSX() {
  int fd = fioOpen("rom0:PSXVER", FIO_O_RDONLY);
  if (fd < 0)
    return 0;
  fioClose(fd);

  // PSX
  settings.patcherFlags |= FLAG_PSX;

#ifndef HOSD
  // Load XFROM modules
  SifLoadModule("rom0:PDEV9", 0, NULL);
  SifLoadModule("rom0:PFLASH", 0, NULL);
  SifLoadModule("rom0:PXFROMMAN", 0, NULL);

  // Read config file
  loadConfig();
  showSplash();
  // Load OSDR into memory
  if ((fd = loadOSDR()))
    return 1; // Indicate that the console is PSX
#endif

  switchPSX();

  return 1;
}
