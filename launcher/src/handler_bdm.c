#include "common.h"
#include "errno.h"
#include "init.h"
#include "loader.h"
#include <fcntl.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <string.h>

#define BDM_MAX_DEVICES 2

// Launches ELF from BDM device
int handleBDM(DeviceType device, int argc, char *argv[]) {
  printf("path: %s\n", argv[0]);
  if ((argv[0] == 0) || (strlen(argv[0]) < 5)) {
    msg("BDM: invalid argument\n");
    return -EINVAL;
  }

  // Build ELF path
  char *elfPath = normalizePath(argv[0], device);
  if (!elfPath)
    return -ENODEV;

  // Initialize device modules
  int res = initModules(device);
  if (res)
    return res;

  // Extract mountpoint for probing
  char *mountpoint = strchr(elfPath, ':');
  char probeMountpoint[10] = {0};
  if (!mountpoint)
    return -EINVAL;
  int mountpointLen = mountpoint - elfPath + 1;

  // Check for wildcard
  mountpoint = strrchr(elfPath, '?');

  // Try all possible mountpoints while decreasing the number of wait
  // attempts for each consecutive device to reduce init times
  int delayAttempts = DELAY_ATTEMPTS; // Max number of attempts
  for (int i = 0; i < BDM_MAX_DEVICES; i++) {
    // Build mountpoint path
    if (mountpoint) { // Handle wildcard paths
      *mountpoint = i + '0';
    } else if (i != 0)
      break; // Break early if the path is not a wildcard path

    strncpy(probeMountpoint, elfPath, mountpointLen);
    while (delayAttempts != 0) {
      // Try to open the file to make sure it exists
      res = open(probeMountpoint, O_DIRECTORY | O_RDONLY);
      if (res >= 0) {
        // If mountpoint is available
        close(res);

        // Jump to launch if file exists
        if (!(res = tryFile(elfPath)))
          goto found;
        // Else, try next device
        break;
      }
      // If the mountpoint is unavailable, delay and decrement the number of wait attempts
      sleep(1);
      delayAttempts--;
    }
    // No more mountpoints available
    if (res < 0)
      break;
  }
  return -ENODEV;

found:
  argv[0] = elfPath;
  return LoadELFFromFile(argc, argv);
}
