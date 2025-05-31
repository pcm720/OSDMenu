#include "patches_common.h"
#include "patterns_ps1drv.h"
#include "settings.h"
#include <osd_config.h>
#include <debug.h>

//
// PS1DRV config patch
// Sets kernel PS1DRV configuration to values set in OSDMENU.CNF if not initialized
//
// The proper way to do this would be to write to Mechacon NVRAM,
// but doing this would not only increase the binary size due to libcdvd, but can also be dangerous
//
#ifndef HOSD
static uint32_t ps1drvFlagsAddr = 0x1f1224;
#else
static uint32_t ps1drvFlagsAddr = 0x1f1284;
#endif

static void (*origSetOSDConfig)(void) = NULL;

void setOSDConfig() {
  // Execute the original function
  origSetOSDConfig();

  ConfigParam osdConfig;
  GetOsdConfigParam(&osdConfig);

  // Return if ps1drvConfig is already initialized
  if (osdConfig.ps1drvConfig)
    return;

  if (settings.patcherFlags & FLAG_PS1DRV_FAST)
    osdConfig.ps1drvConfig |= 1;
  if (settings.patcherFlags & FLAG_PS1DRV_SMOOTH)
    osdConfig.ps1drvConfig |= 0x10;

  // Update kernel config
  SetOsdConfigParam(&osdConfig);
  // Update OSD flags (cosmetic)
  _sw(osdConfig.ps1drvConfig, ps1drvFlagsAddr);
}

void patchPS1DRVConfig(uint8_t *osd) {
  // Find the target function
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternSetOSDConfig, (uint8_t *)patternSetOSDConfig_mask, sizeof(patternSetOSDConfig));
  if (!ptr)
    return;
  ptr += 12;

  // Store the address of the original function
  origSetOSDConfig = (void *)((_lw((uint32_t)ptr) & 0x03ffffff) << 2);
  // Replace the original function
  _sw((0x0c000000 | ((uint32_t)setOSDConfig >> 2)), (uint32_t)ptr); // jal setOSDConfigToKernel
}

#ifndef HOSD
// As the protokernel OSD patching function runs after all the OSD init code,
// PS1DRV config can be patched without injections
static uint32_t ps1drvFlagsAddrProto = 0x001c9ba4;

void patchPS1DRVConfigProtokernel() {
  ConfigParam osdConfig;
  GetOsdConfigParam(&osdConfig);

  // Return if ps1drvConfig is already initialized
  if (osdConfig.ps1drvConfig)
    return;

  if (settings.patcherFlags & FLAG_PS1DRV_FAST)
    osdConfig.ps1drvConfig |= 1;
  if (settings.patcherFlags & FLAG_PS1DRV_SMOOTH)
    osdConfig.ps1drvConfig |= 0x10;

  // Update kernel config
  SetOsdConfigParam(&osdConfig);
  // Update OSD flags (cosmetic)
  _sw(osdConfig.ps1drvConfig, ps1drvFlagsAddrProto);
}
#endif
