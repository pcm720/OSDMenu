#include "common.h"
#include "handlers.h"
#include "loader.h"
#include <fcntl.h>
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

// Reduce binary size by disabling unneeded functionality
void _libcglue_timezone_update() {}
void _libcglue_rtc_update() {}
PS2_DISABLE_AUTOSTART_PTHREAD();

launcherOptions settings;

int main(int argc, char *argv[]) {
  // Process global options
  settings.flags = 0;
  settings.gsmArgument = NULL;
  settings.deviceHint = Device_MemoryCard;
  settings.mcHint = 0;
  settings.dev9ShutdownType = ShutdownType_All;

  // Try to guess the device type using argv[0]
  if (!strncmp(argv[0], "mc1", 3))
    settings.mcHint = 1;
  if (!strncmp(argv[0], "pfs", 3) || !strncmp(argv[0], "hdd", 3))
    settings.deviceHint = Device_APA;

  argc = parseGlobalFlags(argc, argv);

#ifdef OSDM
  if (!strncmp("osdm", argv[0], 4)) {
    settings.flags |= FLAG_BOOT_OSD;
    fail("Failed to launch %s: %d", argv[0], handleOSDM(argc, argv));
  }
  if (!strncmp("cdrom", argv[0], 5))
    fail("Failed to launch %s: %d", argv[0], handleCDROM(argc, argv));
#endif

  if ((argc < 2) || (argv[1][0] == '\0')) { // argv[1] can be empty when launched from OPL
#ifdef APA
    if (strstr(argv[0], ":PATINFO"))
      // Handle "hdd0:<partition>:PATINFO" paths
      fail("PATINFO failed: %d", handlePATINFO(argc, argv));
#endif

    // Try to quickboot with paths from .CNF located at the current working directory
    fail("Quickboot failed: %d", handleQuickboot(argv[0]));
  }

  // Remove launcher path from arguments
  argc--;
  argv = &argv[1];
  if (!argv[0])
    fail("Invalid argv[0]");

  char *p = strrchr(argv[0], '.');
  if (p && (!strcmp(p, ".cfg") || !strcmp(p, ".CFG") || !strcmp(p, ".CNF") || !strcmp(p, ".cnf")))
    // If argv[1] is a CNF/CFG file, try to load it
    fail("Quickboot failed: %d", handleQuickboot(argv[0]));

  fail("Failed to launch %s: %d", argv[0], launchPath(argc, argv));
}
