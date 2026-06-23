#include "defaults.h"
#include "gs.h"
#include "init.h"
#include "launcher.h"
#include "osdr.h"
#include "patches_common.h"
#include "patches_osdmenu.h"
#include "psx.h"
#include "settings.h"
#include "splash.h"
#include <kernel.h>
#include <libcdvd-common.h>
#include <osd_config.h>
#include <ps2sdkapi.h>
#include <sifrpc-common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tamtypes.h>
#define NEWLIB_PORT_AWARE
#include "fileio.h"

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

#ifndef HOSD
// OSDMenu
int main(int argc, char *argv[]) {
  // Clear memory while avoiding the embedded data in the OSD memory region
  memset((void *)EXTRA_SECTION_END, 0, USER_MEM_END_ADDR - EXTRA_SECTION_END);
#ifdef EMBED_CNF
  // Relocate the CNF file to the memory unused by the OSD code
  memcpy((void *)(EXTRA_RELOC_ADDR + size_launcher_elf), (void *)embedded_cnf, size_embedded_cnf);
  embedded_cnf_addr = (void *)(EXTRA_RELOC_ADDR + size_launcher_elf);
#endif

  // Load needed modules
  initModules();
  // Set OSDMenu & OSDSYS default settings for configurable items
  initConfig();

  // Guess MC slot from argv[0]
  if (!strncmp(argv[0], "mc0", 3))
    settings.mcSlot = 0;
  else if (!strncmp(argv[0], "mc1", 3))
    settings.mcSlot = 1;
  else if (!strncmp(argv[0], "xfrom", 5))
    settings.mcSlot = 2;

  // Check for PSX
  int isPSX = initPSX();
  if (!isPSX) {
    // If not, read config file
    loadConfig();
    // Try to load OSDR
    if (loadOSDR())
      Exit(-1);
  }

  // Unpack OSDSYS resource bundle
  int unpackRes = unpackOSDR();
  if (isPSX && unpackRes)
    // Critical error for PSX
    Exit(-1);

  GSVideoMode vmode = GS_MODE_NTSC; // Use NTSC by default

  // Respect preferred mode
  if (!settings.videoMode) {
    // If mode is not set, read console region from ROM
    if (settings.romver[4] == 'E')
      vmode = GS_MODE_PAL;
  } else if (settings.videoMode == GS_MODE_PAL)
    vmode = GS_MODE_PAL;

#ifdef ENABLE_SPLASH
  gsDisplaySplash(vmode);
#else
  gsInit(vmode);
  gsClearScreen();
#endif

  // MBROWS exists only on protokernel systems
  int fd = fioOpen("rom0:MBROWS", FIO_O_RDONLY);
  if (fd >= 0) {
    fioClose(fd);
    // Apply kernel patches for early kernels
    InitOsd();
    // If OSDR was not loaded, run OSDSYS from ROM
    if (unpackRes)
      launchProtokernelOSDSYS();
  }

  if (!unpackRes) {
    // Use OSDSYS from OSDR
    patchExecuteOSDSYS((void *)0x200000, NULL, argc, argv);
    Exit(-1);
  }

  // Execute OSDSYS from ROM
  // Relocate the embedded launcher to the memory unused by the OSD unpacker
  memcpy((void *)EXTRA_RELOC_ADDR, (void *)launcher_elf, size_launcher_elf);
  launcher_elf_addr = (void *)EXTRA_RELOC_ADDR;

  launchOSDSYS(argc, argv);

  Exit(-1);
}
#else
// HOSDMenu

#include <fcntl.h>
#include <fileXio_rpc.h>

#define RECOVERY_PAYLOAD_PATH "usb:/RECOVERY.ELF"

int checkFile(char *path);

int main(int argc, char *argv[]) {
  // Clear memory while avoiding the embedded data in the OSD memory region
  memset((void *)EXTRA_SECTION_END, 0, USER_MEM_END_ADDR - EXTRA_SECTION_END);
  // Relocate the embedded launcher and the legacy ps2atad module to the memory unused by the OSD code
  memcpy((void *)EXTRA_RELOC_ADDR, (void *)launcher_elf, size_launcher_elf);
  launcher_elf_addr = (void *)EXTRA_RELOC_ADDR;
  memcpy((void *)(EXTRA_RELOC_ADDR + size_launcher_elf), (void *)legacy_ps2atad_irx, size_legacy_ps2atad_irx);
  legacy_ps2atad_irx_addr = (void *)(EXTRA_RELOC_ADDR + size_launcher_elf);

  // Set FMCB & OSDSYS default settings for configurable items
  initConfig();

  if ((argc > 1) && !strcmp(argv[argc - 1], "-mbrboot")) {
    // Skip the full init and just initialize fileXio if the last argument is -mbrboot
    shortInit();
    argc--;
  } else {
    if (initModules())
      // Launch recovery payload on fail
      launchPayload(RECOVERY_PAYLOAD_PATH);
  }

  int fd = checkFile("rom0:PSXVER");
  if (fd >= 0)
    // Assume the loader has switched PSX into PS2 mode
    settings.patcherFlags |= FLAG_PSX;

  if (fileXioMount("pfs0:", HOSD_CONF_PARTITION, 0))
    launchPayload(RECOVERY_PAYLOAD_PATH);

  // Read config file
  loadConfig();

  fileXioUmount("pfs0:");

  if (fileXioMount("pfs0:", HOSD_SYS_PARTITION, 0))
    launchPayload(RECOVERY_PAYLOAD_PATH);

  GSVideoMode vmode = GS_MODE_NTSC; // Use NTSC by default

  // Respect preferred mode
  if (!settings.videoMode) {
    // If mode is not set, read console region from ROM
    if (settings.romver[4] == 'E')
      vmode = GS_MODE_PAL;
  } else if (settings.videoMode == GS_MODE_PAL)
    vmode = GS_MODE_PAL;

#ifdef ENABLE_SPLASH
  gsDisplaySplash(vmode);
#else
  gsInit(vmode);
  gsClearScreen();
#endif

  // Check if HDD OSD executable exists
  int haveOSD = checkFile("pfs0:/osd100/hosdsys.elf");
  if (haveOSD < 0)
    haveOSD = checkFile("pfs0:/osd100/OSDSYS_A.XLF");

  if (haveOSD >= 0)
    launchOSDSYS(argc, argv);

  // Fallback to RECOVERY_PAYLOAD_PATH
  fileXioUmount("pfs0:");
  launchPayload(RECOVERY_PAYLOAD_PATH);
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
#endif
