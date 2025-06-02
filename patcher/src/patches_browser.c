#include "loader.h"
#include "patches_common.h"
#include "patterns_browser.h"
#include <debug.h>
#include <string.h>

//
// Browser application launch patch
// Allows launching SAS-compatible apps and ELFs from HDD or memory cards
//

// Common variables and functions
char pathBuf[100];
// Sets up browser for exiting to main menu (clock)
// Verified exit types for OSDSYS:
// 1 - SCE logo
// 2 - Main menu (clock)
// 3 - Browser main screen
// 4 - Red error screen
// 5 - Reloads browser and resets SPU?
static void (*setupExitToPreviousModule)(int exitType) = NULL;
// Sets up variables for the main menu thread
static void (*exitToPreviousModule)() = NULL;

// Launches path from pathBuf if set
// Otherwise executes the original function
void exitToPreviousModuleCustom() {
  if (pathBuf[0] != '\0')
    launchItem(pathBuf);

  exitToPreviousModule();
}

#ifndef HOSD
//
// OSDMenu browser application launch patch
// Swaps file properties and Copy/Delete menus around and launches an app when pressing Enter
//
// 1. Replace the function that opens Copy/Delete or properties submenu for memory card icon
//    This code checks the directory name and, if it contains _ at index 3, equals BOOT or ends with .ELF/.elf,
//    builds the path into pathBuf and calls setupExitToPreviousModule to exit to the main menu.
//
//    Otherwise, it simply swaps around submenus and calls the original function.
//
// 3. Inject custom code into the function that exits to main menu
//    This code calls launchItem instead of the original function if pathBuf is not empty
//

// Sets up submenu data
// entryProps — a pointer to save file properties. Contains icon properties, save date, file size and path relative to mc0/mc1
//   Browser entry icon offsets:
//    0x90  - icon flags (save icon/disc); 0x40 - disc flag
//    0x114 - pointer to some data structure
//   Some data structure offsets:
//    0x124 - device number (2 - mc0, 3/6 - mc1)
// fileSubmenuType — indicates submenu type, where 0 is the Copy/Delete menu and 1 is save file properties­
static void (*browserDirSubmenuSetup)(uint8_t *entryProps, uint8_t fileSubmenuType) = NULL;

void browserDirSubmenuSetupCustom(uint8_t *entryProps, uint8_t fileSubmenuType) {
  if (fileSubmenuType == 1) { // Swap functions around so "Option" button triggers the Copy/Delete menu
    browserDirSubmenuSetup(entryProps, 0);
    return;
  }

  // Get the pointer to device properties starting at offset 0x114
  uint32_t mcNumber = _lw((uint32_t)entryProps + 0x114);
  // Get the device number from offset 0x124
  mcNumber = _lw(mcNumber + 0x124);

  // Translate the memory card number
  // The memory card number OSDSYS uses equals 2 for mc0 and 6 for mc1 (3 for mc1 on protokernels).
  switch (mcNumber) {
  case 6: // mc1
    mcNumber -= 3;
  case 3:
  case 2: // mc0
    mcNumber = mcNumber - 2;

    // Find the path in icon properties starting from offset 0x160
    char *stroffset = (char *)entryProps + 0x160;
    while (*stroffset != '/')
      stroffset++;

    // Launch item if directory name has _ at index 3, equals BOOT or if file name ends with .elf/.ELF
    char *ext = strrchr(stroffset, '.');
    if (!ext && ((stroffset[4] == '_') || !strcmp(stroffset, "/BOOT")))
      goto setupLaunch;
    else if (ext && (!strcmp(ext, ".elf") || !strcmp(ext, ".ELF")))
      goto setupLaunch;

    goto setup;

  setupLaunch:
    // Assemble the path
    pathBuf[0] = '\0';
    strcat(pathBuf, "mc?:");
    strcat(pathBuf, stroffset);
    if (!ext)
      // Add "title.cfg" to SAS paths
      strcat(pathBuf, "/title.cfg");
    pathBuf[2] = mcNumber + '0';

    // Call exit function to exit to clock
    setupExitToPreviousModule(2);
    return;
  }

setup:
  browserDirSubmenuSetup(entryProps, 1);
  return;
}

// Browser application launch patch
void patchBrowserApplicationLaunch(uint8_t *osd, int isProtokernel) {
  // Protokernel browser code starts at ~0x700000
  uint32_t osdOffset = (isProtokernel) ? (PROTOKERNEL_MENU_OFFSET + 0x100000) : 0;

  // Find setupExitToPreviousModule address
  uint8_t *ptr = findPatternWithMask(osd + osdOffset, 0x100000, (uint8_t *)patternSetupExitToPreviousModule,
                                     (uint8_t *)patternSetupExitToPreviousModule_mask, sizeof(patternSetupExitToPreviousModule));
  if (!ptr)
    return;

  setupExitToPreviousModule = (void *)ptr;

  // Find exitToPreviousModule address
  ptr = findPatternWithMask(osd + osdOffset, 0x100000, (uint8_t *)patternExitToPreviousModule, (uint8_t *)patternExitToPreviousModule_mask,
                            sizeof(patternExitToPreviousModule));
  if (!ptr || ((_lw((uint32_t)ptr + 0xc) & 0xfc000000) != 0x0c000000))
    return;

  // Get the original function call, save the address and replace it with the custom one
  ptr += 0xc;
  exitToPreviousModule = (void *)((_lw((uint32_t)ptr) & 0x03ffffff) << 2);
  _sw((0x0c000000 | ((uint32_t)exitToPreviousModuleCustom >> 2)), (uint32_t)ptr); // jal exitToPreviousModuleCustom

  // Find browserDirSubmenuSetup calls
  ptr = findPatternWithMask(osd + osdOffset, 0x100000, (uint8_t *)patternBrowserMainLevelHandler, (uint8_t *)patternBrowserMainLevelHandler_mask,
                            sizeof(patternBrowserMainLevelHandler));
  if (!ptr || ((_lw((uint32_t)ptr + 4) & 0xfc000000) != 0x0c000000))
    return;

  // Store the pointer
  uint32_t tmp = (uint32_t)ptr + 4;

  // Get the original function call and save the address
  browserDirSubmenuSetup = (void *)((_lw(tmp) & 0x03ffffff) << 2);

  if (!osdOffset) {
    // On newer ROMS, there might be two browserDirSubmenuSetup calls
    while (_lw((uint32_t)ptr) != _lw(tmp)) {
      ptr -= 4;

      if ((tmp - (uint32_t)ptr) >= 0x150) {
        // Failed to find the second call
        ptr = NULL;
        break;
      }
    }
    // Replace the function call
    if (ptr)
      _sw((0x0c000000 | ((uint32_t)browserDirSubmenuSetupCustom >> 2)), (uint32_t)ptr); // jal browserDirSubmenuSetupCustom
  }

  // Replace the function call
  _sw((0x0c000000 | ((uint32_t)browserDirSubmenuSetupCustom >> 2)), tmp); // jal browserDirSubmenuSetupCustom
}

#else
//
// HOSDMenu browser application launch patch
// Adds the "Enter" option for ELF files or SAS-compliant applications
//
// 1. Inject custom code into the function that generates icon data
//    This code checks the directory name and sets "application" flag (0x40) if it contains _ at index 3, equals BOOT or ends with .ELF/.elf
// 2. Inject custom code into the function that sets up data for exiting to main menu
//    This code checks the icon data and writes the ELF path into pathBuf if all conditions are met
// 3. Inject custom code into the function that exits to main menu
//    This code calls launchItem instead of the original function if pathBuf is not empty
//
// Browser entry icon offsets:
//  0x90  - icon flags
//  0x134 - pointer to parent device data
//  0x1C0 - dir/file name
// Browser device data offsets:
//  0x124 - device number (2 - mc0, 6 - mc1, >10 = directories on HDD)
//  0x12C - device name or directory name in __common

// Original functions
void (*buildIconData)(uint8_t *iconDataLocation, uint8_t *deviceDataLocation) = NULL;

void buildIconDataCustom(uint8_t *iconDataLocation, uint8_t *deviceDataLocation) {
  // Call the original function
  buildIconData(iconDataLocation, deviceDataLocation);

  // Get directory path
  char *dirPath = (char *)(iconDataLocation + 0x1c0);
  if (!dirPath)
    return;

  // Set executable flag if directory name has _ at index 3, equals BOOT or if file name ends with .elf/.ELF
  char *ext = strrchr(dirPath, '.');
  if (!ext && ((dirPath[4] == '_') || !strcmp(dirPath, "/BOOT")))
    *(iconDataLocation + 0x90) |= 0x40;
  else {
    if (ext && (!strcmp(ext, ".elf") || !strcmp(ext, ".ELF")))
      *(iconDataLocation + 0x90) |= 0x40;
  }
}

void setupExitToPreviousModuleCustom(int appType) {
  uint32_t iconPropertiesPtr = 0;
  uint32_t deviceDataPtr = 0;
  // Get the pointer to currently selected item from $s2
  asm volatile("move %0, $s2\n\t" : "=r"(iconPropertiesPtr)::);

  char *targetName = NULL;
  if (iconPropertiesPtr) {
    // Get the pointer to device properties starting at offset 0x134
    deviceDataPtr = _lw(iconPropertiesPtr + 0x134);
    // Find the path in icon properties starting from offset 0x160
    targetName = (char *)iconPropertiesPtr + 0x1c0;
  }

  if (appType || !deviceDataPtr || !targetName)
    goto out;

  pathBuf[0] = '\0';

  // Get the device number and build the path
  uint32_t devNumber = _lw(deviceDataPtr + 0x124);
  switch (devNumber) {
  case 6:
    // mc1
    devNumber -= 3;
  case 2:
    // mc0
    devNumber -= 2;
    pathBuf[0] = '\0';
    strcat(pathBuf, "mc?:");
    pathBuf[2] = devNumber + '0';
    strcat(pathBuf, targetName);
    break;
  default:
    if (devNumber < 11)
      goto out;

    // Assume the target is one of directories in hdd0:__common
    char *commonDirName = (char *)deviceDataPtr + 0x12c;
    if (!commonDirName)
      goto out;

    strcat(pathBuf, "hdd0:__common:pfs:");
    strcat(pathBuf, commonDirName);
    if (targetName[0] != '/')
      strcat(pathBuf, "/");

    strcat(pathBuf, targetName);
    break;
  }

  // Append title.cfg for SAS applications
  targetName = strrchr(targetName, '.');
  if (!targetName || (strcmp(targetName, ".ELF") && strcmp(targetName, ".elf")))
    strcat(pathBuf, "/title.cfg");

out:
  setupExitToPreviousModule(appType);
  return;
}

// Browser application launch patch
void patchBrowserApplicationLaunch(uint8_t *osd) {
  // Find buildIconData address
  uint8_t *ptr1 =
      findPatternWithMask(osd, 0x100000, (uint8_t *)patternBuildIconData, (uint8_t *)patternBuildIconData_mask, sizeof(patternBuildIconData));
  if (!ptr1)
    return;

  // Find exitToPreviousModule address
  uint8_t *ptr2 = findPatternWithMask(osd, 0x100000, (uint8_t *)patternExitToPreviousModule, (uint8_t *)patternExitToPreviousModule_mask,
                                      sizeof(patternExitToPreviousModule));
  if (!ptr2)
    return;

  // Find setupExitToPreviousModule address
  uint8_t *ptr3 = findPatternWithMask(osd, 0x100000, (uint8_t *)patternSetupExitToPreviousModule, (uint8_t *)patternSetupExitToPreviousModule_mask,
                                      sizeof(patternSetupExitToPreviousModule));
  if (!ptr3)
    return;

  ptr1 += 0x4;
  buildIconData = (void *)((_lw((uint32_t)ptr1) & 0x03ffffff) << 2);
  _sw((0x0c000000 | ((uint32_t)buildIconDataCustom >> 2)), (uint32_t)ptr1); // jal browserDirSubmenuInitViewCustom

  ptr2 += 0xc;
  exitToPreviousModule = (void *)((_lw((uint32_t)ptr2) & 0x03ffffff) << 2);
  _sw((0x0c000000 | ((uint32_t)exitToPreviousModuleCustom >> 2)), (uint32_t)ptr2); // jal exitToPreviousModuleCustom

  setupExitToPreviousModule = (void *)((_lw((uint32_t)ptr3) & 0x03ffffff) << 2);
  _sw((0x0c000000 | ((uint32_t)setupExitToPreviousModuleCustom >> 2)), (uint32_t)ptr3); // jal setupExitToPreviousModuleCustom
}

// Homebrew atad driver with LBA48 support
extern unsigned char legacy_ps2atad_irx[] __attribute__((aligned(16)));
extern uint32_t size_legacy_ps2atad_irx;

// Patches newer 48-bit capable atad driver into HDD-OSD
void patchATAD() {
  // Check for ELF magic at offset 0x0058cd80 and replace the atad driver
  if (_lw(0x0058cd80) == 0x464c457f) {
    memset((void *)0x0058cd80, 0x0, 0x0058fbfd - 0x0058cd80);
    memcpy((void *)0x0058cd80, legacy_ps2atad_irx, size_legacy_ps2atad_irx);
  }
}
#endif
