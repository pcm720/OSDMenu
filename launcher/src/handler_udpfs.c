#include "common.h"
#include "init.h"
#include <fcntl.h>
#include <ps2sdkapi.h>
#include <string.h>

// Launches ELF from UDPFS device
int handleUDPFS(int argc, char *argv[]) {
  if ((argv[0] == 0) || (strlen(argv[0]) < 5)) {
    msg("UDPFS: invalid argument\n");
    return -EINVAL;
  }

  // Initialize device modules
  int res = initModules(Device_UDPFS);
  if (res)
    return res;

  int delayAttempts = DELAY_ATTEMPTS; // Max number of attempts
  while (delayAttempts != 0) {
    // Try to open the file to make sure the device exists
    if (!(res = tryFile(argv[0])))
      return LoadELFFromFile(argc, argv);
    sleep(1);
    delayAttempts--;
  }
  return -ENODEV;
}
