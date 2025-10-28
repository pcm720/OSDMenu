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
  int res = initPFS(NULL, 0, Device_None);
  if (res)
    return res;

  char *titleID = NULL;
  LoadOptions *lopts = parsePATINFO(argc, argv, &titleID);
  if (!lopts)
    return -ENOENT;

  argc = parseGlobalFlags(lopts->argc, lopts->argv);

  if (titleID) {
    updateLaunchHistory(titleID, 0);
    free(titleID);
  }

  if (settings.flags & FLAG_BOOT_PATINFO) {
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
