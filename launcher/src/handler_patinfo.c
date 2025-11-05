#include "common.h"
#include "dprintf.h"
#include "game_id.h"
#include "patinfo.h"
#include <ps2sdkapi.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Starts application using data from the HDD partition attribute area header
// Assumes argv[0] is the partition path
int handlePATINFO(int argc, char *argv[]) {
  int res = initPFS(NULL, 0, Device_ATA);
  if (res)
    return res;

  char *titleID = NULL;
  LoadOptions *lopts = parsePATINFO(argc, argv, &titleID);
  if (!lopts)
    return -ENOENT;

  lopts->argc = parseGlobalFlags(lopts->argc, lopts->argv);
  // Apply DEV9 options to other handlers
  settings.dev9ShutdownType = lopts->dev9ShutdownType;

  if (settings.titleID) {
    free(titleID);
    titleID = settings.titleID;
    DPRINTF("Title ID is overriden to %s\n", settings.titleID);
  }

  if (titleID) {
    updateLaunchHistory(titleID, ((settings.titleID) ? 1 : 0));
    free(titleID);
  }

  if (settings.flags & FLAG_BOOT_PATINFO) {
    DPRINTF("BOOT_PATINFO flag is set, ignoring bootPath\n");
    // Ignore the argv[0] as it points to another copy of the launcher
    if (lopts->ioprpPath)
      free(lopts->ioprpPath);

    lopts->argc--;
    free(lopts->argv[0]);
    lopts->argv = &lopts->argv[1];
    return launchPath(lopts->argc, lopts->argv);
  }

  if (strncmp(lopts->argv[0], "hdd0", 3)) {
    if (lopts->ioprpPath)
      free(lopts->ioprpPath);

    return launchPath(lopts->argc, lopts->argv);
  }

  return loadELF(lopts);
}
