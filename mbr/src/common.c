#include "config.h"
#include "crypto.h"
#include "defaults.h"
#include "game_id.h"
#include "hdd.h"
#include "history.h"
#include "loader.h"
#include <debug.h>
#include <fcntl.h>
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdlib.h>
#include <string.h>

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

// Displays the fatal error message on the screen
void fatalMsg(char *msg) {
  // Display the error message
  initScreen();
  scr_printf("\t\nFatal error:\n\t%s\n", msg);

  // Delay
  int ret = 0x20000000;
  while (ret--)
    asm("nop\nnop\nnop\nnop");
}

// Starts HOSDMenu or HDD-OSD.
// Assumes the system partition is already mounted and argv has space for the additional -mbrboot argument.
// Will unmount the partition.
int startHOSDSYS(int argc, char *argv[]) {
  char *hosdsysPath = NULL;
  if (checkFile("pfs0:" HOSDSYS_PFS_PATH_1) >= 0)
    hosdsysPath = SYSTEM_PARTITION ":pfs:" HOSDSYS_PFS_PATH_1;
  else if (checkFile("pfs0:" HOSDSYS_PFS_PATH_2) >= 0)
    hosdsysPath = SYSTEM_PARTITION ":pfs:" HOSDSYS_PFS_PATH_2;

  umountPFS();

  if (!hosdsysPath)
    return -ENOENT;

  argv[0] = hosdsysPath;

  if (!mountPFS(HOSD_SYS_PARTITION)) {
    int res = checkFile("pfs0:" HOSD_PFS_PATH);
    umountPFS();
    if (res >= 0) {
      // Boot HOSDMenu
      argv[0] = HOSD_SYS_PARTITION ":pfs:" HOSD_PFS_PATH;

      argv[argc] = strdup("-mbrboot");

      LoadOptions opts = {
          .dev9ShutdownType = ShutdownType_None,
          .argc = argc + 1,
          .argv = argv,
      };
      loadELF(&opts);
      return -ENOENT;
    }
  }

  // Boot HOSDSYS
  if (!(settings.flags & FLAG_DISABLE_GAMEID) && (settings.flags & FLAG_APP_GAMEID))
    gsDisplayGameID("Browser 2.0");

  LoadOptions opts = {
      .argc = argc,
      .argv = argv,
  };
  loadELF(&opts);
  return -ENOENT;
}

// Attempts to launch PSBBN, HOSDMenu or HOSDSYS. Falls back to OSDSYS
void execOSD(int argc, char *argv[]) {
  if (mountPFS(SYSTEM_PARTITION)) {
    ExecOSD(argc, argv);
    return;
  }

  // Ensure we always work with initialized argv array
  char **nargv = malloc((argc + 2) * sizeof(char *)); // + 2 to store nargv[0] and possible HOSDMenu -mbrboot argument
  int nargc = argc + 1;
  nargv[0] = NULL;
  if (argv && argc > 0)
    for (int i = 0; i < argc; i++)
      nargv[i + 1] = argv[i];

  if ((settings.flags & FLAG_PREFER_BBN) && (checkFile("pfs0:" OSDBOOT_PFS_PATH) >= 0)) {
    umountPFS();

    if (!(settings.flags & FLAG_DISABLE_GAMEID) && (settings.flags & FLAG_APP_GAMEID))
      gsDisplayGameID("SCPN_601.60");

    // Start PSBBN
    nargv[0] = SYSTEM_PARTITION ":pfs:" OSDBOOT_PFS_PATH;
    if (nargc > 1) // Encrypt arguments
      nargv = encryptOSDBOOTArgs(&nargc, nargv);

    LoadOptions opts = {
        .argc = nargc,
        .argv = nargv,
    };
    loadELF(&opts);
    return;
  }

  startHOSDSYS(nargc, nargv);

  // Else, exec OSDSYS
  // Shutdown dev9
  shutdownDEV9();
  ExecOSD(argc, argv);
  return;
}

// Shuts down HDD, dev9 and exits to OSD with arguments
void bootFailWithArgs(char *msg, int argc, char *argv[]) {
  fatalMsg(msg);

  // Execute OSDSYS with the "BootError" argument
  execOSD(argc, argv);
  __builtin_trap();
}

// Shuts down HDD, dev9 and exits to OSD
void bootFail(char *msg) {
  // Execute OSD with the "BootError" argument
  char *args[] = {"BootError"};
  bootFailWithArgs(msg, 1, args);
}
