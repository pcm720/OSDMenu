#include "cnf.h"
#include "dprintf.h"
#include "game_id.h"
#include "handlers.h"
#include "init.h"
#include "loader.h"
#include <ctype.h>
#include <debug.h>
#include <fcntl.h>
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <hdd-ioctl.h>
#include <io_common.h>

static int isScreenInited = 0;
char pathbuffer[PATH_MAX];

void initScreen() {
  if (isScreenInited)
    return;

  init_scr();
  scr_setCursor(0);
  scr_printf(".\n\n\n\n"); // To avoid messages being hidden by overscan
  isScreenInited = 1;
}

// Prints a message to the screen and console
void msg(const char *str, ...) {
  va_list args;
  va_start(args, str);

  initScreen();

  scr_vprintf(str, args);

  va_end(args);
}

// Prints a message to the screen and console and exits
void fail(const char *str, ...) {
  va_list args;
  va_start(args, str);

  initScreen();

  scr_vprintf(str, args);

  va_end(args);

  sleep(10);

  char *argv[1] = {"BootIllegal"};
  ExecOSD(1, argv);
}

// Tests if file exists by opening it
int tryFile(char *filepath) {
  int fd = open(filepath, O_RDONLY);
  if (fd < 0) {
    return fd;
  }
  close(fd);
  return 0;
}

// Attempts to launch ELF from device and path in path
int launchPath(int argc, char *argv[]) {
  int ret = 0;
  switch (guessDeviceType(argv[0])) {
  case Device_MemoryCard:
    ret = handleMC(argc, argv);
    break;
#ifdef MMCE
  case Device_MMCE:
    ret = handleMMCE(argc, argv);
    break;
#endif
#ifdef USB
  case Device_USB:
    ret = handleBDM(Device_USB, argc, argv);
    break;
#endif
#ifdef ATA
  case Device_ATA:
    ret = handleBDM(Device_ATA, argc, argv);
    break;
#endif
#ifdef MX4SIO
  case Device_MX4SIO:
    ret = handleBDM(Device_MX4SIO, argc, argv);
    break;
#endif
#ifdef ILINK
  case Device_iLink:
    ret = handleBDM(Device_iLink, argc, argv);
    break;
#endif
#ifdef UDPBD
  case Device_UDPBD:
    ret = handleBDM(Device_UDPBD, argc, argv);
    break;
#endif
#ifdef APA
  case Device_APA:
    if (strstr(argv[0], ":PATINFO"))
      ret = handlePATINFO(argc, argv);
    else
      ret = handlePFS(argc, argv);
    break;
#endif
#ifdef CDROM
  case Device_CDROM:
    ret = handleCDROM(argc, argv);
    break;
#endif
  case Device_ROM:
    execROMPath(argc, argv);
    break;
  default:
    return -ENODEV;
  }

  return ret;
}

// Attempts to guess device type from path
DeviceType guessDeviceType(char *path) {
  if (!strncmp("mc", path, 2)) {
    return Device_MemoryCard;
#ifdef MMCE
  } else if (!strncmp("mmce", path, 4)) {
    return Device_MMCE;
#endif
#ifdef USB
  } else if (!strncmp("mass", path, 4) || !strncmp("usb", path, 3)) {
    return Device_USB;
#endif
#ifdef ATA
  } else if (!strncmp("ata", path, 3)) {
    return Device_ATA;
#endif
#ifdef MX4SIO
  } else if (!strncmp("mx4sio", path, 6)) {
    return Device_MX4SIO;
#endif
#ifdef ILINK
  } else if (!strncmp("ilink", path, 5)) {
    return Device_iLink;
#endif
#ifdef UDPBD
  } else if (!strncmp("udpbd", path, 5)) {
    return Device_UDPBD;
#endif
#ifdef APA
  } else if (!strncmp("hdd", path, 3)) {
    return Device_APA;
#endif
#ifdef CDROM
  } else if (!strncmp("cdrom", path, 5)) {
    return Device_CDROM;
#endif
  } else if (!strncmp("rom", path, 3))
    return Device_ROM;

  return Device_None;
}

// Attempts to convert launcher-specific path into a valid device path
char *normalizePath(char *path, DeviceType type) {
  pathbuffer[0] = '\0';
  switch (type) {
  case Device_APA:
    if (!strncmp("hdd", path, 3)) {
      char *pfsPath = strstr(path, ":pfs:");
      if (pfsPath) {
        path = pfsPath + 5;
      } else {
        char *pfsPath = strchr(path, '/');
        if (pfsPath)
          path = pfsPath;
      }
    }
    if (path[0] == '/')
      strcat(pathbuffer, PFS_MOUNTPOINT);
    else
      strcat(pathbuffer, PFS_MOUNTPOINT "/");
  case Device_MemoryCard:
  case Device_MMCE:
  case Device_CDROM:
    strncat(pathbuffer, path, PATH_MAX - 6);
    break;
  // BDM
  case Device_USB:
  case Device_ATA:
  case Device_MX4SIO:
  case Device_iLink:
  case Device_UDPBD:
    char devNumber = path[4];
    // Get relative ELF path from argv[0]
    path = strchr(path, ':');
    if (!path)
      return NULL;

    path++;

    strcpy(pathbuffer, BDM_MOUNTPOINT);
    if ((devNumber > '0') && (devNumber <= '9'))
      pathbuffer[4] = devNumber;

    if (path[0] != '/')
      strcat(pathbuffer, "/");
    strncat(pathbuffer, path, PATH_MAX - sizeof(BDM_MOUNTPOINT) - 1);
    break;
  default:
    return NULL;
  }
  return pathbuffer;
}

// Mounts the partition specified in path
int mountPFS(char *path) {
#ifndef APA
  return -ENODEV;
#else
  // Extract partition path
  char *filePath = strstr(path, ":pfs:");
  char pathSeparator = '\0';
  if (filePath || (filePath = strchr(path, '/'))) {
    // Terminate the partition path
    pathSeparator = filePath[0];
    filePath[0] = '\0';
  }

  // Mount the partition
  DPRINTF("Mounting %s to %s\n", path, PFS_MOUNTPOINT);
  int res = fileXioMount(PFS_MOUNTPOINT, path, FIO_MT_RDONLY);
  if (pathSeparator != '\0')
    filePath[0] = pathSeparator; // Restore the path
  if (res)
    return -ENODEV;

  return 0;
#endif
}

// Initializes APA-formatted HDD and mounts the partition if specified
int initPFS(char *path, int clearSPU, DeviceType additionalDevices) {
#ifndef APA
  return -ENODEV;
#else
  int res;
  // Reset IOP
  if ((res = initModules(Device_APA | additionalDevices, clearSPU)))
    return res;

  // Wait for IOP to initialize device driver
  DPRINTF("Waiting for HDD to become available\n");
  for (int attempts = 0; attempts < DELAY_ATTEMPTS; attempts++) {
    res = open("hdd0:", O_DIRECTORY | O_RDONLY);
    if (res >= 0) {
      close(res);
      break;
    }
    sleep(1);
  }
  if (res < 0)
    return -ENODEV;

  if (!path)
    return 0;
  return mountPFS(path);
#endif
}

// Unmounts the partition
void deinitPFS() {
#ifdef APA
  fileXioDevctl(PFS_MOUNTPOINT, PDIOC_CLOSEALL, NULL, 0, NULL, 0);
  fileXioSync(PFS_MOUNTPOINT, FXIO_WAIT);
  fileXioUmount(PFS_MOUNTPOINT);
#endif
}

// Puts HDD in idle mode and powers off the dev9 device
void shutdownDEV9() {
#if defined(APA) || defined(ATA)
  // Unmount the partition (if mounted)
  fileXioUmount("pfs0:");
  // Immediately put HDDs into idle mode
  fileXioDevctl("hdd0:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
  fileXioDevctl("hdd1:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
  // Turn off dev9
  fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0);
#endif
}

// Parses the launcher argv for global flags.
// Returns the new argc
int parseGlobalFlags(int argc, char *argv[]) {
  if (argc < 2)
    return argc;

  char *valuePtr = NULL;
  for (int i = argc - 1; i > 0; i--) {
    // Find the start of the value
    valuePtr = strchr(argv[i], '=');
    if (valuePtr) {
      // Trim whitespace and terminate the value
      do {
        valuePtr++;
      } while (isspace((int)*valuePtr));
      valuePtr[strcspn(valuePtr, "\r\n")] = '\0';
    }

    if (valuePtr && !strncmp(argv[i], "-gsm=", 5)) {
      // eGSM argument
      settings.gsmArgument = strdup(valuePtr);
      DPRINTF("Applying eGSM options: %s\n", settings.gsmArgument);
      argc--;
    } else if (!strcmp(argv[i], "-osd")) {
      settings.flags |= FLAG_BOOT_OSD;
      DPRINTF("Setting OSD flag\n");
      argc--;
    } else if (!strcmp(argv[i], "-appid")) {
      settings.flags |= FLAG_APP_GAMEID;
      DPRINTF("Enabling game ID for apps\n");
      argc--;
    } else if (!strcmp(argv[i], "-patinfo")) {
      settings.flags |= FLAG_BOOT_PATINFO;
      DPRINTF("Setting PATINFO flag\n");
      argc--;
    } else if (valuePtr && !strncmp(argv[i], "-titleid=", 9)) {
      settings.titleID = strdup(valuePtr);
      DPRINTF("Using custom title ID: %s\n", settings.titleID);
      argc--;
    } else if (valuePtr && !strncmp(argv[i], "-dev9", 5)) {
      if (!strcmp(valuePtr, "NICHDD"))
        settings.dev9ShutdownType = ShutdownType_None;
      else if (!strcmp(valuePtr, "NIC"))
        settings.dev9ShutdownType = ShutdownType_HDD;
      DPRINTF("DEV9 Shutdown Type: %d\n", settings.dev9ShutdownType);
      argc--;
    } else
      break; // Exit to preserve application arguments
  }

  return argc;
}

int LoadELFFromFile(int argc, char *argv[]) {
  if (settings.titleID || (settings.flags & FLAG_APP_GAMEID)) {
    char *titleID = settings.titleID;
    if (!titleID)
      titleID = generateTitleID(argv[0]);
    if (titleID) {
      DPRINTF("Title ID is %s\n", titleID);
      gsDisplayGameID(titleID);
      free(titleID);
    }
  }

  LoadOptions opts = {
      .argc = argc,
      .argv = argv,
      .dev9ShutdownType = settings.dev9ShutdownType,
  };
  if (settings.gsmArgument)
    opts.eGSM = settings.gsmArgument;

  return loadELF(&opts);
}
