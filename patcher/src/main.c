#include "defaults.h"
#include "gs.h"
#include "init.h"
#include "launcher.h"
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
  // Clear memory while avoiding the embedded data in the OSD memory region
  memset((void *)EXTRA_SECTION_END, 0, USER_MEM_END_ADDR - EXTRA_SECTION_END);
  // Relocate the embedded launcher to the memory unused by the OSD unpacker
  memcpy((void *)EXTRA_RELOC_ADDR, (void *)launcher_elf, size_launcher_elf);
  launcher_elf_addr = (void *)EXTRA_RELOC_ADDR;
#ifdef EMBED_CNF
  // Relocate the CNF file to the memory unused by the OSD code
  memcpy((void *)(EXTRA_RELOC_ADDR + size_launcher_elf), (void *)embedded_cnf, size_embedded_cnf);
  embedded_cnf_addr = (void *)(EXTRA_RELOC_ADDR + size_launcher_elf);
#else
  // Load needed modules
  initModules();
#endif

  // Set FMCB & OSDSYS default settings for configurable items
  initConfig();

  // Determine from which mc slot FMCB was booted
  if (!strncmp(argv[0], "mc0", 3))
    settings.mcSlot = 0;
  else if (!strncmp(argv[0], "mc1", 3))
    settings.mcSlot = 1;

  // Read config file
  loadConfig();

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

  int fd = fioOpen("rom0:MBROWS", FIO_O_RDONLY);
  if (fd >= 0) {
    // MBROWS exists only on protokernel systems
    fioClose(fd);
    launchProtokernelOSDSYS();
  } else
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
  // Relocate the embedded launcher to the memory unused by the OSD code
  void *relocAddr = (void *)(EXTRA_RELOC_ADDR + launcher_elf - EXTRA_SECTION_START);
  memcpy(relocAddr, (void *)launcher_elf, size_launcher_elf);
  launcher_elf_addr = relocAddr;

  if ((argc > 1) && !strcmp(argv[argc - 1], "-mbrboot")) {
    // Skip the full init and just initialize fileXio if the last argument is -mbrboot
    shortInit();
    argc--;
  } else {
    // Else, do the full init
    if (initModules())
      // Launch recovery payload on fail
      launchPayload(RECOVERY_PAYLOAD_PATH);
  }

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
