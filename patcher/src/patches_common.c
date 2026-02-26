#include "defaults.h"
#include "init.h"
#include "launcher.h"
#include "patches_fmcb.h"
#include "patches_osdmenu.h"
#include "patterns_common.h"
#include "settings.h"
#include <kernel.h>
#include <loadfile.h>
#include <stdlib.h>
#include <string.h>

// OSDSYS deinit functions
static void (*osdsysDeinit)(uint32_t flags) = NULL;
#ifdef HOSD
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

static void (*sceUmount)(char *mountpoint) = NULL;
static void (*sceRemove)(char *mountpoint) = NULL;
#endif

// Searches for byte pattern in memory
uint8_t *findPatternWithMask(uint8_t *buf, uint32_t bufsize, uint8_t *bytes, uint8_t *mask, uint32_t len) {
  uint32_t i, j;

  for (i = 0; i < bufsize - len; i++) {
    for (j = 0; j < len; j++) {
      if ((buf[i + j] & mask[j]) != bytes[j])
        break;
    }
    if (j == len)
      return &buf[i];
  }
  return NULL;
}

// Searches for string in memory
char *findString(const char *string, char *buf, uint32_t bufsize) {
  uint32_t i;
  const char *s, *p;

  for (i = 0; i < bufsize; i++) {
    s = string;
    for (p = buf + i; *s && *s == *p; s++, p++)
      ;
    if (!*s)
      return (buf + i);
  }
  return NULL;
}

// Applies patches and executes OSDSYS
void patchExecuteOSDSYS(void *epc, void *gp, int argc, char *argv[]) {
  // If custom menu is enabled, apply menu patch
  if (settings.patcherFlags & FLAG_CUSTOM_MENU) {
    patchMenu((uint8_t *)epc);
    patchMenuDraw((uint8_t *)epc);
    patchMenuInfiniteScrolling((uint8_t *)epc, 0);
    patchMenuButtonPanel((uint8_t *)epc);
  }

  // Apply browser application launch patch
  patchBrowserApplicationLaunch((uint8_t *)epc, 0);

  // Apply version menu patch
  patchVersionInfo((uint8_t *)epc);

  // Apply OSD region patch
  patchOSDRegion((uint8_t *)epc);

  if ((settings.patcherFlags & FLAG_PS1DRV_FAST) || (settings.patcherFlags & FLAG_PS1DRV_SMOOTH))
    // Patch PS1DRV config
    patchPS1DRVConfig((uint8_t *)epc);

  switch (settings.videoMode) {
  case GS_MODE_PAL:
    patchVideoMode((uint8_t *)epc, settings.videoMode);
    break;
  case GS_MODE_DTV_480P:
  case GS_MODE_DTV_1080I:
    patchGSVideoMode((uint8_t *)epc, settings.videoMode); // Apply 480p or 1080i patch
  case GS_MODE_NTSC:
    patchVideoMode((uint8_t *)epc, GS_MODE_NTSC); // Force NTSC
  }

  // Apply skip disc patch
  if (settings.patcherFlags & FLAG_SKIP_DISC)
    patchSkipDisc((uint8_t *)epc);

  // Apply disc launch patch to forward disc launch to the launcher
  patchDiscLaunch((uint8_t *)epc);

  // Find OSDSYS deinit function
  uint8_t *ptr =
      findPatternWithMask((uint8_t *)epc, 0x100000, (uint8_t *)patternOSDSYSDeinit, (uint8_t *)patternOSDSYSDeinit_mask, sizeof(patternOSDSYSDeinit));
  if (ptr)
    osdsysDeinit = (void *)ptr;

  int n = 0;
  char *args[10];
#ifndef HOSD
  // OSDSYS
  // The path is intentionally not rom0:OSDSYS since some modchips seem to hook into ExecPS2
  // and break OSDMenu-patched OSDSYS when the argv[0] is 'rom0:OSDSYS'
  args[n++] = "rom0:";

  if (argc > 1)
    // Passthrough the original arguments
    for (int i = 1; i < argc; i++)
      args[n++] = strdup(argv[i]);

  if (settings.patcherFlags & FLAG_BOOT_BROWSER)
    args[n++] = "BootBrowser"; // Pass BootBrowser to launch internal mc browser
  else if ((settings.patcherFlags & FLAG_SKIP_DISC) || (settings.patcherFlags & FLAG_SKIP_SCE_LOGO))
    args[n++] = "BootClock"; // Pass BootClock to skip OSDSYS intro

  if (findString("SkipMc", (char *)epc, 0x100000)) // Pass SkipMc argument
    args[n++] = "SkipMc";                          // Skip mc?:/BREXEC-SYSTEM/osdxxx.elf update on v5 and above
  else
    // Mangle system update paths to prevent OSDSYS from loading system updates (for ROMs not supporting SkipMc)
    while ((ptr = (uint8_t *)findString("EXEC-SYSTEM", (char *)epc, 0x100000)))
      ptr[2] = '\0';

  if (findString("SkipHdd", (char *)epc, 0x100000)) // Pass SkipHdd argument if the ROM supports it
    args[n++] = "SkipHdd";                          // Skip HDDLOAD on v5 and above
  else
    patchSkipHDD((uint8_t *)epc); // Skip HDD patch for earlier ROMs

#else
  // HDD OSD
  args[n++] = "hdd0:__system:pfs:/osd100/hosdsys.elf";

  if (argc > 1)
    // Passthrough the original arguments
    for (int i = 1; i < argc; i++)
      args[n++] = strdup(argv[i]);

  if (settings.patcherFlags & FLAG_BOOT_BROWSER)
    args[n++] = "BootBrowser"; // Pass BootBrowser to launch internal mc browser
  else if ((settings.patcherFlags & FLAG_SKIP_DISC) || (settings.patcherFlags & FLAG_SKIP_SCE_LOGO))
    args[n++] = "BootClock"; // Pass BootClock to skip OSDSYS intro

  // Update IOP modules
  patchHOSDModules();

  // Patch-in support for hidden partitions
  patchBrowserHiddenPartitions();

  // Find sceRemove function
  ptr = findPatternWithMask((uint8_t *)epc, 0x100000, (uint8_t *)patternSCERemove, (uint8_t *)patternSCERemove_mask, sizeof(patternSCERemove));
  if (ptr)
    sceRemove = (void *)ptr;
  // Find sceUmount function
  ptr = findPatternWithMask((uint8_t *)epc, 0x100000, (uint8_t *)patternSCEUmount, (uint8_t *)patternSCEUmount_mask, sizeof(patternSCEUmount));
  if (ptr)
    sceUmount = (void *)ptr;
#endif

  // Relocate the embedded launcher to avoid OSD code overwriting it
  memcpy((void *)USER_MEM_START_ADDR, launcher_elf_addr, size_launcher_elf);
  launcher_elf_addr = (uint8_t *)USER_MEM_START_ADDR;

  FlushCache(0);
  FlushCache(2);
  ExecPS2(epc, gp, n, args);
  Exit(-1);
}

// Loads OSDSYS from ROM and handles the patching
void launchOSDSYS(int argc, char *argv[]) {
  uint8_t *ptr;
  t_ExecData exec;

#ifndef HOSD
  if (SifLoadElf("rom0:OSDSYS", &exec) || (exec.epc < 0))
    return;
#else
  if (SifLoadElfEncrypted("pfs0:/osd100/hosdsys.elf", &exec) || (exec.epc < 0))
    if (SifLoadElfEncrypted("pfs0:/osd100/OSDSYS_A.XLF", &exec) || (exec.epc < 0))
      return;

  fileXioUmount("pfs0:");
#endif

  // Find the ExecPS2 function in the unpacker starting from 0x100000.
  ptr = findPatternWithMask((uint8_t *)0x100000, 0x1000, (uint8_t *)patternExecPS2, (uint8_t *)patternExecPS2_mask, sizeof(patternExecPS2));
  if (ptr) {
    // If found, patch it to call patchExecuteOSDSYS() function.
    uint32_t instr = 0x0c000000;
    instr |= ((uint32_t)patchExecuteOSDSYS >> 2);
    *(uint32_t *)ptr = instr;
    *(uint32_t *)&ptr[4] = 0;
  }

#ifndef EMBED_CNF
  resetModules();
#endif

  // Execute the OSD unpacker. If the above patching was successful it will
  // call the patchExecuteOSDSYS() function after unpacking.
  ExecPS2((void *)exec.epc, (void *)exec.gp, argc, argv);
  Exit(-1);
}

#ifdef HOSD
#define OHCI_REG_BASE 0xbf801600
#define HcControl (OHCI_REG_BASE + 0x04)
#define HcCommandStatus (OHCI_REG_BASE + 0x08)
#define HcInterruptDisable (OHCI_REG_BASE + 0x14)

#define OHCI_COM_HCR (1 << 0)

// Resets USB host controller by writing directly to OHCI registers
// Avoids IOP memory corruption in the launcher caused by incomplete usbd deinit during IOP reset
void resetOHCI() {
  // Enter the kernel mode to access memory-mapped I/O
  ee_kmode_enter();
  *(volatile uint32_t *)HcInterruptDisable = ~0;
  *(volatile uint32_t *)HcControl &= ~0x3Cu;

  // Wait ~2 milliseconds (replicates usbd behavior)
  int i = 0x500;
  while (i--)
    asm("nop\nnop\nnop\nnop");

  // Send HC Reset command
  *(volatile uint32_t *)HcCommandStatus = OHCI_COM_HCR;
  *(volatile uint32_t *)HcControl = 0;
  for (i = 0; i < 1000; i++)
    if (!(*(volatile uint32_t *)HcCommandStatus & OHCI_COM_HCR))
      break;

  // Exit the kernel mode
  ee_kmode_exit();
  return;
}

#endif

// Calls OSDSYS deinit function
void deinitOSDSYS() {
#ifdef HOSD
  if (sceRemove)
    sceRemove("hdd0:_tmp");
  if (sceUmount)
    sceUmount("pfs1:");
#endif

  if (osdsysDeinit)
    osdsysDeinit(1);

#ifdef HOSD
  resetOHCI();
#endif
}

#ifndef HOSD
//
// Protokernel functions
//
static void (*protoSceSifLoadModule)(const char *path, int arg_len, const char *args) = NULL;
// Loads rom0:CLEARSPU module
void protokernelDeinit(uint32_t flags) { protoSceSifLoadModule("rom0:CLEARSPU", 0, 0); }

// Applies patches and executes OSDSYS
static void *protoEPC;
void applyProtokernelPatches() {
  if (settings.patcherFlags & FLAG_CUSTOM_MENU) {
    // If custom menu is enabled, apply menu patch
    patchMenuProtokernel((uint8_t *)protoEPC);
    patchMenuDrawProtokernel((uint8_t *)protoEPC);
    patchMenuInfiniteScrolling((uint8_t *)protoEPC, 1);
  }

  // Apply browser application launch patch
  patchBrowserApplicationLaunch((uint8_t *)protoEPC, 1);

  // Apply version menu patch
  patchVersionInfoProtokernel((uint8_t *)protoEPC);

  if ((settings.patcherFlags & FLAG_PS1DRV_FAST) || (settings.patcherFlags & FLAG_PS1DRV_SMOOTH))
    // Patch PS1DRV config
    patchPS1DRVConfigProtokernel();

  // Patch the video mode if required
  switch (settings.videoMode) {
  case GS_MODE_DTV_480P:
  case GS_MODE_DTV_1080I:
    patchGSVideoModeProtokernel((uint8_t *)protoEPC, settings.videoMode); // Apply 480p or 1080i patch
  default:
  }

  // Apply disc launch patch to forward disc launch to the launcher
  patchDiscLaunchProtokernel((uint8_t *)protoEPC);

  FlushCache(0);
  FlushCache(2);
}

// Loads OSDSYS from ROM and injects the patching function into OSDSYS
void launchProtokernelOSDSYS() {
  t_ExecData exec;

  if (SifLoadElf("rom0:OSDSYS", &exec) || (exec.epc < 0))
    return;

  // Find OSDSYS init function
  uint8_t *ptr = findPatternWithMask((uint8_t *)exec.epc, 0x100000, (uint8_t *)patternOSDSYSProtokernelInit,
                                     (uint8_t *)patternOSDSYSProtokernelInit_mask, sizeof(patternOSDSYSProtokernelInit));
  if (!ptr)
    return;

  // Inject patching function
  uint32_t tmp = 0x08000000;
  tmp |= ((uint32_t)applyProtokernelPatches >> 2);
  _sw(tmp, (uint32_t)(ptr + 0x3c)); // j applyProtokernelPatches

  // Find OSDSYS SifLoadModule function for deinit
  ptr = findPatternWithMask((uint8_t *)exec.epc, 0x100000, (uint8_t *)patternProtokernelSifLoadModule,
                            (uint8_t *)patternProtokernelSifLoadModule_mask, sizeof(patternProtokernelSifLoadModule));
  if (ptr) {
    // Set the deinit function
    protoSceSifLoadModule = (void *)ptr + 0x4;
    osdsysDeinit = protokernelDeinit;
  }

  // Set OSDSYS address
  protoEPC = (void *)exec.epc;

  // Mangle system update paths to prevent OSDSYS from loading system updates
  while ((ptr = (uint8_t *)findString("EXEC-SYSTEM", (char *)protoEPC, 0x100000)))
    ptr[2] = '\0';

  int n = 0;
  char *args[2];
  args[n++] = "rom0:OSDSYS";
  if (settings.patcherFlags & FLAG_BOOT_BROWSER)
    args[n++] = "BootBrowser"; // Pass BootBrowser to launch internal mc browser
  else if ((settings.patcherFlags & FLAG_SKIP_DISC) || (settings.patcherFlags & FLAG_SKIP_SCE_LOGO))
    args[n++] = "BootClock"; // Pass BootClock to skip OSDSYS intro

  // Execute OSDSYS
#ifndef EMBED_CNF
  resetModules();
#endif

  FlushCache(0);
  FlushCache(2);
  ExecPS2((void *)exec.epc, (void *)exec.gp, n, args);
  Exit(-1);
}
#endif
