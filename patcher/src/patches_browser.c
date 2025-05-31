#include "loader.h"
#include "patches_common.h"
#include "patterns_browser.h"
#include <debug.h>
#include <string.h>

#ifndef HOSD
//
// OSDMenu browser application launch patch
// Swaps file properties and Copy/Delete menus around and launches an app when pressing Enter
// if a title.cfg file is present in the save file directory
//

// Initializes submenu data
// entryProps — a pointer to save file properties. Contains icon properties, save date, file size and path relative to mc0/mc1
// fileSubmenuType — indicates submenu type, where 0 is the Copy/Delete menu and 1 is save file properties­
static void (*browserDirSubmenuInitView)(uint8_t *entryProps, uint8_t fileSubmenuType) = NULL;
// Returns directory size. Called only after submenu is entered and only once: the result is cached in entry properties and the buffer
// is reused for subsequent sceMcGetDir calls. To persist the app indicator, browserGetMcDirSizeCustom stores 0xAA at *(entryProps - 0x2)
static int (*browserGetMcDirSize)(void) = NULL;
// The address of sceMcGetDir result buffer
static uint32_t sceMcGetDirResultAddr = 0x0;

// Selected MC index offset from $gp, signed
int16_t selectedMCOffset = 0;
// Entry properties address for the currently selected entry
// The most significant bit is used to signal the browserGetMcDirSizeCustom function that it needs to launch the app immediately
uint32_t entryPropsAddr = 0;
// Path buffer
char pathBuf[100];

// This function is always executed first and called every time a submenu is opened
void browserDirSubmenuInitViewCustom(uint8_t *entryProps, uint8_t fileSubmenuType) {
  // Store the address for browserGetMcDirSizeCustom
  entryPropsAddr = (uint32_t)entryProps;

  if (fileSubmenuType == 1) { // Swap functions around so "Option" button triggers the Copy/Delete menu
    browserDirSubmenuInitView(entryProps, 0);
    return;
  }

  // Get memory card number by reading address relative to $gp
  int mcNumber;
  asm volatile("addu $t0, $gp, %1\n\t" // Add the offset to the gp register
               "lw %0, 0($t0)"         // Load the word at the offset
               : "=r"(mcNumber)
               : "r"((int32_t)selectedMCOffset) // Cast the type to avoid GCC using lhu instead of lh
               : "$t0");

  // Translate the memory card number
  // The memory card number OSDSYS uses equals 2 for mc0 and 6 for mc1 (3 for mc1 on protokernels).
  switch (mcNumber) {
  case 6: // mc1
    mcNumber -= 3;
  case 3:
  case 2: // mc0
    mcNumber = mcNumber - 2;

    // Set the most significant bit to indicate that immediate launch is needed
    entryPropsAddr |= (1 << 31);

    // Find the path in icon properties starting from offset 0x160
    char *stroffset = (char *)entryProps + 0x160;
    while (*stroffset != '/')
      stroffset++;

    // Assemble the path
    pathBuf[0] = '\0';
    strcat(pathBuf, "mc?:");
    strcat(pathBuf, stroffset);
    strcat(pathBuf, "/title.cfg");
    pathBuf[2] = mcNumber + '0';

    if (*(entryProps - 0x2) == 0xAA)
      // If icon is an app, launch the app
      launchItem(pathBuf);
    // Else, the app will be launched by the browserGetMcDirSizeCustom function
  }

  browserDirSubmenuInitView(entryProps, 1);
}

// This function is always executed after browserDirSubmenuInitViewCustom and never called again
// once it returns the directory size until the user goes back to the memory card select screen.
// If immediate launch is requested, launches the app if it has title.cfg entry in sceMcGetDir result
// Otherwise, puts 0xAA at (entryProps - 0x2) for browserDirSubmenuInitViewCustom
int browserGetMcDirSizeCustom() {
  int res = browserGetMcDirSize();
  // This function returns -8 until sceMcSync returns the result
  if ((res != -8)) {
    // Each entry occupies 0x40 bytes, with entry name starting at offset 0x20
    char *off = (char *)(sceMcGetDirResultAddr + 0x20);
    while (off[0] != '\0') {
      if (!strcmp("title.cfg", off)) {
        // If immediate launch is requested, launch the app
        if (entryPropsAddr & (1 << 31))
          launchItem(pathBuf);

        // Else, put app marker (0xAA) to *(entryProps - 0x2)
        *(((uint8_t *)((entryPropsAddr) & 0x7FFFFFFF)) - 0x2) = 0xAA;
        off += 0x40;
        break;
      }
      off += 0x40;
    }
    // Zero-out the result buffer to avoid false positives since we don't know the number of entries
    memset((void *)sceMcGetDirResultAddr, 0, ((uint32_t)off - sceMcGetDirResultAddr));
  }
  return res;
}

// Browser application launch patch
void patchBrowserApplicationLaunch(uint8_t *osd, int isProtokernel) {
  // Protokernel browser code starts at ~0x700000
  uint32_t osdOffset = (isProtokernel) ? (PROTOKERNEL_MENU_OFFSET + 0x100000) : 0;

  // Find the target function
  uint8_t *ptr = findPatternWithMask(osd + osdOffset, 0x100000, (uint8_t *)patternBrowserFileMenuInit, (uint8_t *)patternBrowserFileMenuInit_mask,
                                     sizeof(patternBrowserFileMenuInit));

  if (!ptr || ((_lw((uint32_t)ptr + 4 * 4) & 0xfc000000) != 0x0c000000))
    return;
  ptr += 4 * 4;

  // Get the original function call and save the address
  browserDirSubmenuInitView = (void *)((_lw((uint32_t)ptr) & 0x03ffffff) << 2);

  // Find the selected memory card offset
  uint8_t *ptr2 = findPatternWithMask((uint8_t *)browserDirSubmenuInitView, 0x500, (uint8_t *)patternBrowserSelectedMC,
                                      (uint8_t *)patternBrowserSelectedMC_mask, sizeof(patternBrowserSelectedMC));
  if (!ptr2)
    return;
  // Store memory card offset
  selectedMCOffset = _lw((uint32_t)ptr2) & 0xffff;

  // Find the target function
  ptr2 = findPatternWithMask(osd + osdOffset, 0x100000, (uint8_t *)patternBrowserGetMcDirSize, (uint8_t *)patternBrowserGetMcDirSize_mask,
                             sizeof(patternBrowserGetMcDirSize));
  if (!ptr2)
    return;
  // Get the original function call and save the address
  browserGetMcDirSize = (void *)((_lw((uint32_t)ptr2) & 0x03ffffff) << 2);

  if (isProtokernel)
    // Use fixed address for protokernels. The call tree is way more complicated on these,
    // so it's not worth the trouble since the address is identical for both 1.00 and 1.01
    sceMcGetDirResultAddr = 0x008f66d0;
  else {
    // Trace the sceMcGetDir result buffer address
    // From the browserGetMcDirSize function, get the address of the next function call
    //  that retrieves directory size for mc0/mc1. This function sends sceMcGetDir request
    //  to the libmc worker thread. The buffer address is stored in $t0.
    // This function loads the upper part of the address to $v0
    // adds the base offset to $s1, adds constant offset and puts the result into $t0.
    // So, to get the address:
    // 1. Find the first load into $v0 (lui   v0,????)
    // 2. Find the first add into $s1  (addiu s1,v0,????)
    // 3. Add the constant offset 0xc38 (same for all ROM versions >=1.10)
    uint32_t offset = (uint32_t)browserGetMcDirSize;

    // Find the first function call in browserGetMcDirSize and get the function address
    while ((_lw(offset) & 0xfc000000) != 0x0c000000)
      offset += 4;
    offset = (_lw((uint32_t)offset) & 0x03ffffff) << 2;

    // Initialize with fixed offset
    sceMcGetDirResultAddr = 0xc38;

    // Search for lui v0,???? instruction to get the upper part
    while ((_lw(offset) & 0x3c020000) != 0x3c020000)
      offset += 4;
    sceMcGetDirResultAddr |= (_lw(offset) & 0xffff) << 16;

    // Search for addiu s1,v0,???? to get the lower part (might be negative)
    while ((_lw(offset) & 0x24510000) != 0x24510000)
      offset += 4;
    sceMcGetDirResultAddr += (int32_t)((int16_t)(_lw(offset) & 0xffff));
  }

  // Replace original functions
  _sw((0x0c000000 | ((uint32_t)browserDirSubmenuInitViewCustom >> 2)), (uint32_t)ptr); // jal browserDirSubmenuInitViewCustom
  _sw((0x0c000000 | ((uint32_t)browserGetMcDirSizeCustom >> 2)), (uint32_t)ptr2);      // jal browserGetMcDirSizeCustom
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

// Path buffer
char pathBuf[100];
// Original functions
void (*buildIconData)(uint8_t *iconDataLocation, uint8_t *deviceDataLocation) = NULL;
void (*exitToPreviousModule)() = NULL;
void (*setupExitToPreviousModule)(int appType) = NULL;

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

void exitToPreviousModuleCustom() {
  if (pathBuf[0] != '\0')
    launchItem(pathBuf);

  exitToPreviousModule();
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
