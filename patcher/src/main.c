#include "defaults.h"
#include "gs.h"
#include "init.h"
#include "loader.h"
#include "patches_common.h"
#include "settings.h"
#include "splash.h"
#include <kernel.h>
#include <osd_config.h>
#include <ps2sdkapi.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

#ifndef HOSD
// OSDMenu

#include <fileio.h>

int main(int argc, char *argv[]) {
  // Clear memory
  memset((void *)USER_MEM_START_ADDR, 0, USER_MEM_END_ADDR - USER_MEM_START_ADDR);

  // Load needed modules
  initModules();

  // Set FMCB & OSDSYS default settings for configurable items
  initConfig();

  // Determine from which mc slot FMCB was booted
  if (!strncmp(argv[0], "mc0", 3))
    settings.mcSlot = 0;
  else if (!strncmp(argv[0], "mc1", 3))
    settings.mcSlot = 1;

  // Read config file
  loadConfig();

#ifdef ENABLE_SPLASH
  GSVideoMode vmode = GS_MODE_NTSC; // Use NTSC by default

  // Respect preferred mode
  if (!settings.videoMode) {
    // If mode is not set, read console region from ROM
    if (settings.romver[4] == 'E')
      vmode = GS_MODE_PAL;
  } else if (settings.videoMode == GS_MODE_PAL)
    vmode = GS_MODE_PAL;

  gsDisplaySplash(vmode);
#endif

  int fd = fioOpen("rom0:MBROWS", FIO_O_RDONLY);
  if (fd >= 0) {
    // MBROWS exists only on protokernel systems
    fioClose(fd);
    launchProtokernelOSDSYS();
  } else
    launchOSDSYS();

  Exit(-1);
}
#else
// HOSDMenu

#include <fcntl.h>
#include <fileXio_rpc.h>
#include <libpad.h>

#define RECOVERY_PAYLOAD_PATH "usb:/RECOVERY.ELF"

void handleCustomPayload(int haveOSD);
int checkFile(char *path);
int readPad();

int main(int argc, char *argv[]) {
  // Clear memory while avoiding the embedded launcher
  memset((void *)USER_MEM_START_ADDR, 0, (int)&launcher_elf - USER_MEM_START_ADDR);
  memset(((void *)&launcher_elf + size_launcher_elf), 0, USER_MEM_END_ADDR - ((int)&launcher_elf + size_launcher_elf));

  // Load needed modules
  if (initModules())
    launchPayload(RECOVERY_PAYLOAD_PATH);

  // Set FMCB & OSDSYS default settings for configurable items
  initConfig();

  if (fileXioMount("pfs0:", HOSD_CONF_PARTITION, 0))
    launchPayload(RECOVERY_PAYLOAD_PATH);

  // Read config file
  loadConfig();

  fileXioUmount("pfs0:");

  if (fileXioMount("pfs0:", HOSD_SYS_PARTITION, 0))
    launchPayload(RECOVERY_PAYLOAD_PATH);

#ifdef ENABLE_SPLASH
  GSVideoMode vmode = GS_MODE_NTSC; // Use NTSC by default

  // Respect preferred mode
  if (!settings.videoMode) {
    // If mode is not set, read console region from ROM
    if (settings.romver[4] == 'E')
      vmode = GS_MODE_PAL;
  } else if (settings.videoMode == GS_MODE_PAL)
    vmode = GS_MODE_PAL;

  gsDisplaySplash(vmode);
#endif

  // Check if HDD OSD executable exists
  int haveOSD = checkFile("pfs0:" HOSD_HDDOSD_PATH);

  // If custom payload path is set, attempt to launch it
  if (settings.customPayload[0] != '\0')
    handleCustomPayload(haveOSD);

  if (haveOSD >= 0)
    launchOSDSYS();

  // Fallback to RECOVERY_PAYLOAD_PATH
  fileXioUmount("pfs0:");
  launchPayload(RECOVERY_PAYLOAD_PATH);
}

// Checks if payload exists and attempts to boot it if
// HDD OSD is missing or if requested by the user
void handleCustomPayload(int haveOSD) {
  // Extract relative path from hdd0: path and build PFS path
  char payloadPath[50] = {0};
  strcat(payloadPath, "pfs0:");
  char *filePath = strstr(settings.customPayload, ":pfs:");
  if (filePath)
    filePath += 5;
  else if (!(filePath = strchr(settings.customPayload, '/')))
    return;

  strcat(payloadPath, filePath);

  // Make sure payload exists
  if (checkFile(payloadPath) < 0)
    return;

  // Fallback to payload if OSDSYS_A.XLF is missing
  if (haveOSD < 0)
    goto launch;

  // Otherwise, read the pad
  int doBoot = readPad();
  if (!doBoot && (settings.patcherFlags & FLAG_BOOT_PAYLOAD))
    // If button is not pressed and the flag is set
    goto launch;
  if (doBoot && !(settings.patcherFlags & FLAG_BOOT_PAYLOAD))
    // If button is pressed and the flag is not set
    goto launch;

  return;

launch:
  fileXioUmount("pfs0:");
  launchPayload(settings.customPayload);
}

// Returns >=0 if file exists
int checkFile(char *path) {
  int res = open(path, O_RDONLY);
  if (res < 0) {
    return -1;
  }
  close(res);
  return res;
}

// Initiailizes the pad library and returns 1 if Cross was pressed
int readPad() {
  struct padButtonStatus buttons;
  static char padBuf[256] __attribute__((aligned(64)));

  padInit(0);
  padPortOpen(0, 0, padBuf);
  int padState = 0;
  while ((padState = padGetState(0, 0))) {
    if (padState == PAD_STATE_STABLE)
      break;
    if (padState == PAD_STATE_DISCONN)
      return 0;
  }

  int retries = 10;
  while (retries--) {
    if (padRead(0, 0, &buttons) != 0) {
      uint32_t paddata = 0xffff ^ buttons.btns;
      if (paddata & PAD_CROSS)
        return 1;
    }
  }
  padEnd();
  return 0;
}
#endif
