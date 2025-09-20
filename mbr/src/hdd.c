#include "common.h"
#include "loader.h"
#include <errno.h>
#include <hdd-ioctl.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>

char *fsckPaths[] = {
    "hdd0:__system:pfs:/fsck100/fsck.elf",
    "hdd0:__system:pfs:/fsck100/FSCK_A.XLF",
    "hdd0:__system:pfs:/fsck/fsck.elf",
    "hdd0:__system:pfs:/fsck/FSCK_A.XLF",
};

int mountPFS(char *path);
int umountPFS();

// Puts HDD in idle mode and powers off the dev9 device
void shutdownDEV9() {
  // Unmount the partition (if mounted)
  umountPFS();
  // Immediately put HDDs into idle mode
  fileXioDevctl("hdd0:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
  fileXioDevctl("hdd1:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);

  // Turn off dev9
  fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0);
}

// Runs HDD checks and returns:
// - `-1` on error
// - `0` on success
// - `1` if any of the PFS partitions need checking
int isFsckRequired() {
  int ret = fileXioDevctl("hdd0:", HDIOC_STATUS, NULL, 0, NULL, 0);
  if (ret != 0) {
    printf("HDIOC_STATUS failed\n");
    return -1;
  }

  if ((ret = fileXioDevctl("hdd0:", HDIOC_SMARTSTAT, NULL, 0, NULL, 0)) != 0) {
    printf("HDIOC_SMARTSTAT failed\n");
    return -1;
  }

  if ((ret = fileXioDevctl("hdd0:", HDIOC_GETSECTORERROR, NULL, 0, NULL, 0)) != 0) {
    printf("HDIOC_GETSECTORERROR failed\n");
    return -1;
  }

  char partitionName[64] = "";
  if ((ret = fileXioDevctl("hdd0:", HDIOC_GETERRORPARTNAME, NULL, 0, &partitionName, sizeof(partitionName))) == 0) {
    return 0;
  }

  printf("Filesystem needs checking: %s has errors\n", partitionName);
  return 1;
}

// Finds and launches the fsck utility
void runFsck() {
  char *elfPath = NULL;
  char *arg[1] = {NULL};
  for (int i = 0; i < (sizeof(fsckPaths) / sizeof(char *)); i++) {
    mountPFS(fsckPaths[i]);
    elfPath = (strstr(fsckPaths[i], ":pfs")) + 1;
    if (checkFile(elfPath) >= 0) {
      arg[0] = fsckPaths[i];
      break;
    }

    umountPFS();
  }
  if (!arg[0])
    return;

  umountPFS();
  LoadELFFromFile(0, 1, arg);
}

// Mounts the partition specified in path
int mountPFS(char *path) {
  // Extract partition path
  char *filePath = strstr(path, ":pfs:");
  char pathSeparator = '\0';
  if (filePath || (filePath = strchr(path, '/'))) {
    // Terminate the partition path
    pathSeparator = filePath[0];
    filePath[0] = '\0';
  }

  // Mount the partition
  int res = fileXioMount("pfs0:", path, FIO_MT_RDONLY);
  if (pathSeparator != '\0')
    filePath[0] = pathSeparator; // Restore the path
  if (res)
    return -ENODEV;

  return 0;
}

// Unmounts the currently mounted partition
int umountPFS() { return fileXioUmount("pfs0:"); }
