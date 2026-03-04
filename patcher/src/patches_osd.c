#include "patches_common.h"
#include "patterns_osd.h"
#include "settings.h"
#include <debug.h>

// OSD region patch
void patchOSDRegion(uint8_t *osd) {
  if (settings.region == OSD_REGION_DEFAULT)
    return;

  // Find the get region function
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternOSDRegion, (uint8_t *)patternOSDRegion_mask, sizeof(patternOSDRegion));
  if (!ptr)
    return;

  // ROMVER -> OSD region mapping:
  // 'J' --- 0 (Japan)
  // 'A'/'H' --- 1 (USA)
  // 'E' --- 2 (Europe)
  // 'C' --- 3 (China?)
  // Replace load from memory with just storing our value into v0 register
  uint32_t val = 0x24020000;
  switch (settings.region) {
  case OSD_REGION_JAP:
    val |= 0x0;
    break;
  case OSD_REGION_USA:
    val |= 0x1;
    break;
  case OSD_REGION_EUR:
    val |= 0x2;
    break;
  default:
    return;
  }
  _sw(val, (uint32_t)ptr);
}

// Patches OSD disc launch handling in intro and clock handlers to not enter
// Browser or launch disc when the disc is inserted
void patchOSDAutoDiscHandling(uint8_t *osd) {
  // Find the intro disc handling
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternOSDIntroLaunchDisc, (uint8_t *)patternOSDIntroLaunchDisc_mask,
                                     sizeof(patternOSDIntroLaunchDisc));
  if (!ptr)
    return;

#ifdef HOSD
  // NOP out the disc handling in HDD-OSD main()
  for (uint32_t i = 0; i < (sizeof(patternOSDIntroLaunchDisc) / sizeof(uint32_t)); i++)
    _sw(0x0000000, (uint32_t)ptr + i * 4);
#else
  _sw(0x24020064, (uint32_t)ptr); // Store 0x64 (default value for no disc) into v1

  // Patch the second occurence
  ptr = findPatternWithMask(ptr, 0x200, (uint8_t *)patternOSDIntroLaunchDisc, (uint8_t *)patternOSDIntroLaunchDisc_mask,
                            sizeof(patternOSDIntroLaunchDisc));
  if (ptr)
    _sw(0x24020064, (uint32_t)ptr); // Store 0x64 (default value for no disc) into v1
#endif

  // Find the clock disc handling
  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternOSDClockLaunchDisc, (uint8_t *)patternOSDClockLaunchDisc_mask,
                            sizeof(patternOSDClockLaunchDisc));
  if (!ptr)
    return;

  _sw(0x0000000, (uint32_t)ptr + 0x4); // replace with nop

  // This might appear twice in some ROMs, so make sure we patch both
  ptr = findPatternWithMask(ptr, 0x100, (uint8_t *)patternOSDClockLaunchDisc, (uint8_t *)patternOSDClockLaunchDisc_mask,
                            sizeof(patternOSDClockLaunchDisc));
  if (!ptr)
    return;

  _sw(0x0000000, (uint32_t)ptr + 0x4); // replace with nop
}
