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
