#include "launcher.h"
#include "defaults.h"
#include "gs.h"
#include "init.h"
#include "patches_common.h"
#include "patches_osdmenu.h"
#include "settings.h"
#include <kernel.h>
#include <loadfile.h>
#include <malloc.h>
#include <osd_config.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>

char *argv[7] = {0};
int argc = 0;
int LoadELFFromFile(int argc, char *argv[]);

// Executes the payload by passing it to the launcher
void launchPayload(char *payloadPath) {
  argc = 2;

  argv[0] = "pfs0:";
  argv[1] = payloadPath;

  // Execute the launcher
  LoadELFFromFile(argc, argv);
}

// Executes selected item by passing it to the launcher
void launchItem(char *item) {
  DisableIntc(3);
  DisableIntc(2);

  // SIF operations do not work on Protokernels
  // without terminating all other threads
  uint32_t tID = GetThreadId();
  ChangeThreadPriority(tID, 0);
  CancelWakeupThread(tID);
  for (int i = 0; i < 0x100; i++) {
    if (i != tID) {
      TerminateThread(i);
      DeleteThread(i);
    }
  }

  // Revert video patch and deinit OSDSYS
  restoreGSVideoMode();
  deinitOSDSYS();
  // Clear the screen
  gsInit(settings.videoMode);

  // Reinitialize DMAC, VU0/1, VIF0/1, GIF, IPU
  ResetEE(0x7F);

  FlushCache(0);
  FlushCache(2);

  // Launcher will do the rest of the cleanup

  // Build argv for the launcher
  if (strcmp(item, "cdrom")) {
    if (!strncmp(item, "osdm", 4))
      argv[argc++] = item;
    else {
      argv[argc++] = "";
      argv[argc++] = item;
    }
  } else {
    // Handle CDROM
    argv[argc++] = item;
    argv[argc++] = (settings.patcherFlags & FLAG_SKIP_PS2_LOGO) ? "-nologo" : "";
    argv[argc++] = (!(settings.patcherFlags & FLAG_DISABLE_GAMEID)) ? "" : "-nogameid";
    if (settings.patcherFlags & FLAG_USE_DKWDRV) {
#ifndef HOSD
      if (settings.dkwdrvPath[0] == '\0')
        argv[argc++] = "-dkwdrv";
      else {
        argv[argc++] = malloc(sizeof("-dkwdrv") + strlen(settings.dkwdrvPath) + 1);
        argv[argc][0] = '\0';
        strcat(argv[argc], "-dkwdrv=");
        strcat(argv[argc], settings.dkwdrvPath);
      }
#else // HOSD DKWDRV path is fixed
      argv[argc++] = "-dkwdrv=" HOSD_DKWDRV_PATH;
#endif
      argc++;
    } else {
      // Get OSD config for PS1DRV
      ConfigParam osdConfig;
      GetOsdConfigParam(&osdConfig);
      argv[argc++] = (osdConfig.ps1drvConfig & 0x1) ? "-ps1fast" : "";
      argv[argc++] = (osdConfig.ps1drvConfig & 0x10) ? "-ps1smooth" : "";
      argv[argc++] = (!(settings.patcherFlags & FLAG_PS1DRV_USE_VN)) ? "" : "-ps1vneg";
    }
  }
#ifndef HOSD
  argv[argc++] = "-osd";
#else
  argv[argc++] = "-hosd";
#endif

#ifndef HOSD
  // Reinitialize IOP to a known state on protokernel to avoid ExecPS2 failing
  if ((settings.romver[1] == '1') && (settings.romver[2] == '0')) {
    sceSifInitRpc(0);
    resetModules();
  }
#endif

  // Execute the launcher
  LoadELFFromFile(argc, argv);
  Exit(-1);
}

// Uses the launcher to run the disc
void launchDisc() { launchItem("cdrom"); }

// Uses the launcher to run the HDD app
void launchHDDApplication() {
  // Data is placed at fixed locations.
  // 0x1F009C — device name
  // 0x1F00A4 — partition name
  // 0x1F00C6 — filepath (PATINFO/pfs:)
  char path[1024] = {0};
  snprintf(path, 100, "%s%s:%s", (char *)0x1f009c, (char *)0x1f00a4, (char *)0x1f00c6);
  launchItem(path);
}

//
// All of the following code is a modified version of elf.c from PS2SDK with unneeded bits removed
//

typedef struct {
  uint8_t ident[16]; // struct definition for ELF object header
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf_header_t;

typedef struct {
  uint32_t type; // struct definition for ELF program section header
  uint32_t offset;
  void *vaddr;
  uint32_t paddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  uint32_t align;
} elf_pheader_t;

#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1

// Might be relocated by the HDD OSD patcher
uint8_t *launcher_elf_addr = launcher_elf;

int LoadELFFromFile(int argc, char *argv[]) {
  elf_header_t *eh;
  elf_pheader_t *eph;
  void *pdata;
  int i;

  eh = (elf_header_t *)launcher_elf_addr;
  if (_lw((uint32_t)&eh->ident) != ELF_MAGIC)
    __builtin_trap();

  eph = (elf_pheader_t *)(launcher_elf_addr + eh->phoff);

  // Scan through the ELF program headers and copy them into RAM
  for (i = 0; i < eh->phnum; i++) {
    if (eph[i].type != ELF_PT_LOAD)
      continue;

    pdata = (void *)(launcher_elf_addr + eph[i].offset);
    memcpy(eph[i].vaddr, pdata, eph[i].filesz);
  }

#ifdef HOSD
  // Clear the region of memory used by the launcher
  // for HDD OSD
  // This is important.
  memset((void *)0x100000 + size_launcher_elf, 0, 0x200000);
#endif

  FlushCache(0);
  FlushCache(2);

  return ExecPS2((void *)eh->entry, NULL, argc, argv);
}
