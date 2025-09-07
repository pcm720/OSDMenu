#include "common.h"
#include "defaults.h"
#include "dprintf.h"
#include "handlers.h"
#include <ctype.h>
#include <init.h>
#include <kernel.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Defined in common/defaults.h
char cnfPath[sizeof(CONF_PATH) + 6] = {0};

// Loads ELF specified in OSDMENU.CNF on the memory card or on the APA partition specified in HOSD_CONF_PARTITION
// Supported osdm prefixes are:
// osdm:d0:<item index> — configuration file on mc0
// osdm:d1:<item index> — configuration file on mc1
// osdm:d9:<item index> — configuration file on APA HDD
// osdm:a<HEX-encoded address>:<HEX-encoded file size>:<item index> — configuration file at the memory address
int handleOSDM(int argc, char *argv[]) {
  if (strlen(argv[0]) < 7)
    return -EINVAL;

  int res = 0;
  int target = 0;
  int targetIdx = 0;
  int targetSize = 0;
  switch (argv[0][5]) {
  case 'd':
    // Configuration file on MC/HDD
    res = sscanf(argv[0], "osdm:d%d:%d", &target, &targetIdx);
    break;
  case 'a':
    // Embedded configuration file
    res = sscanf(argv[0], "osdm:a%08X:%08X:%d", &target, &targetSize, &targetIdx);
    break;
  default:
    msg("OSDM: unexpected device type '%c'\n", argv[0][5]);
    return -EINVAL;
  }
  if (res < 2) {
    msg("OSDM: failed to parse the argument: target %d, targetSize %d, targetIdx %d\n", target, targetSize, targetIdx);
    return -EINVAL;
  }

  FILE *file = NULL;
  if (targetSize > 0) {
    // Init file from memory
    file = fmemopen((void *)target, targetSize, "r");
  } else {
    if (target == 9) {
      // Handle HOSDMenu launch
      if ((res = initPFS(HOSD_CONF_PARTITION, 1)))
        return res;

      // Build path to OSDMENU.CNF
      strcat(cnfPath, PFS_MOUNTPOINT);
      strcat(cnfPath, HOSD_CONF_PATH);
    } else if (target < 2) {
      // Handle OSDMenu launch
      int res = initModules(Device_MemoryCard, 1);
      if (res)
        return res;

      // Build path to OSDMENU.CNF
      strcpy(cnfPath, CONF_PATH);

      // Get memory card slot from argv[0] (osdm0/1)
      if (argv[0][4] == '1') {
        // If path is osdm1:, try to get config from mc1 first
        cnfPath[2] = '1';
        if (tryFile(cnfPath)) // If file is not found, revert to mc0
          cnfPath[2] = '0';
      }
    } else {
      msg("OSDM: unexpected device index %d\n", target);
      return -EINVAL;
    }
    // Open the config file
    file = fopen(cnfPath, "r");
  }

  if (!file) {
    msg("OSDM: Failed to open the file: %d\n", errno);
    if (target == 9)
      deinitPFS();
    return -ENOENT;
  }

  // CDROM arguments
  int displayGameID = 1;
  int skipPS2LOGO = 0;
  int useDKWDRV = 0;
  int ps1drvFlags = 0;
  char *dkwdrvPath = NULL;

  if (target == 9)
    // Set DKWDRV path for HOSDMenu
    dkwdrvPath = HOSD_DKWDRV_PATH;

  // Temporary path and argument lists
  linkedStr *targetPaths = NULL;
  linkedStr *targetArgs = NULL;
  int targetArgc = 1; // argv[0] is the ELF path

  char lineBuffer[PATH_MAX] = {0};
  char *valuePtr = NULL;
  char *idxPtr = NULL;
  while (fgets(lineBuffer, sizeof(lineBuffer), file)) { // fgets returns NULL if EOF or an error occurs
    // Find the start of the value
    valuePtr = strchr(lineBuffer, '=');
    if (!valuePtr)
      continue;
    *valuePtr = '\0';

    // Trim whitespace and terminate the value
    do {
      valuePtr++;
    } while (isspace((int)*valuePtr));
    valuePtr[strcspn(valuePtr, "\r\n")] = '\0';

    if (!strncmp(lineBuffer, "path", 4)) {
      // Get the pointer to path?_OSDSYS_ITEM_
      idxPtr = strrchr(lineBuffer, '_');
      if (!idxPtr)
        continue;

      if (atoi(++idxPtr) != targetIdx)
        continue;

      if ((strlen(valuePtr) > 0)) {
        targetPaths = addStr(targetPaths, valuePtr);
      }
      continue;
    }
    if (!strncmp(lineBuffer, "arg", 3)) {
      // Get the pointer to arg?_OSDSYS_ITEM_
      idxPtr = strrchr(lineBuffer, '_');
      if (!idxPtr)
        continue;

      if (atoi(++idxPtr) != targetIdx)
        continue;

      if ((strlen(valuePtr) > 0)) {
        targetArgs = addStr(targetArgs, valuePtr);
        targetArgc++;
      }
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_skip_ps2logo", 18)) {
      skipPS2LOGO = atoi(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_disable_gameid", 20)) {
      if (atoi(valuePtr))
        displayGameID = 0;
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_use_dkwdrv", 16)) {
      useDKWDRV = 1;
      continue;
    }
    if (!(target == 9) && !strncmp(lineBuffer, "path_DKWDRV_ELF", 15)) {
      dkwdrvPath = strdup(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "ps1drv_enable_fast", 18)) {
      if (atoi(valuePtr))
        ps1drvFlags |= CDROM_PS1_FAST;
      continue;
    }
    if (!strncmp(lineBuffer, "ps1drv_enable_smooth", 20)) {
      if (atoi(valuePtr))
        ps1drvFlags |= CDROM_PS1_SMOOTH;
      continue;
    }
    if (!strncmp(lineBuffer, "ps1drv_use_ps1vn", 16)) {
      ps1drvFlags |= CDROM_PS1_VN;
      continue;
    }
  }
  fclose(file);

  if (target == 9)
    deinitPFS();

  if (!targetPaths) {
    msg("OSDM: No paths found for entry %d\n", targetIdx);
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (dkwdrvPath)
      free(dkwdrvPath);
    return -EINVAL;
  }

  // Handle 'OSDSYS' entry
  if (!strcmp(targetPaths->str, "OSDSYS")) {
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (dkwdrvPath)
      free(dkwdrvPath);

    ExecOSD(0, NULL);
  }

  // Handle 'POWEROFF' entry
  if (!strcmp(targetPaths->str, "POWEROFF")) {
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (dkwdrvPath)
      free(dkwdrvPath);
    shutdownPS2();
  }

  // Handle 'cdrom' entry
  if (!strcmp(targetPaths->str, "cdrom")) {
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (!useDKWDRV && dkwdrvPath) {
      free(dkwdrvPath);
      dkwdrvPath = NULL;
    }
    return startCDROM(displayGameID, skipPS2LOGO, ps1drvFlags, dkwdrvPath);
  }

  if (dkwdrvPath)
    free(dkwdrvPath);

  // Build argv, freeing targetArgs
  char **targetArgv = malloc(targetArgc * sizeof(char *));
  linkedStr *tlstr;
  if (targetArgs) {
    tlstr = targetArgs;
    for (int i = 1; i < targetArgc; i++) {
      targetArgv[i] = tlstr->str;
      tlstr = tlstr->next;
      free(targetArgs);
      targetArgs = tlstr;
    }
    free(targetArgs);
  }

  // Try every path
  tlstr = targetPaths;
  while (tlstr) {
    targetArgv[0] = tlstr->str;
    // If target path is valid, it'll never return from launchPath
    DPRINTF("Trying to launch %s\n", targetArgv[0]);
    launchPath(targetArgc, targetArgv);
    free(tlstr->str);
    tlstr = tlstr->next;
    free(targetPaths);
    targetPaths = tlstr;
  }
  free(targetPaths);

  msg("OSDM: All paths have been tried\n");
  return -ENODEV;
}
