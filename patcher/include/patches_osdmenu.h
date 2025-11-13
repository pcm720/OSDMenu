#ifndef _PATCHES_OSDMENU_H_
#define _PATCHES_OSDMENU_H_
// OSDMenu patches
#include "gs.h"
#include <stdint.h>

// Extends version menu with custom entries
void patchVersionInfo(uint8_t *osd);

// Overrides SetGsCrt and sceGsPutDispEnv functions to support 480p and 1080i output modes
// ALWAYS call restoreGSVideoMode before launching apps
void patchGSVideoMode(uint8_t *osd, GSVideoMode outputMode);

// Restores SetGsCrt.
// Can be safely called even if GS video mode patch wasn't applied
void restoreGSVideoMode();

// Browser application launch patch
void patchBrowserApplicationLaunch(uint8_t *osd, int isProtokernel);

// Sets kernel PS1DRV configuration to values set in OSDMENU.CNF if not initialized
void patchPS1DRVConfig(uint8_t *osd);

#ifndef HOSD
// Protokernel patches

// Extends version menu with custom entries
void patchVersionInfoProtokernel(uint8_t *osd);

// Overrides SetGsCrt and sceGsPutDispEnv functions to support 480p and 1080i output modes
void patchGSVideoModeProtokernel(uint8_t *osd, GSVideoMode outputMode);

// Sets kernel PS1DRV configuration to values set in OSDMENU.CNF if not initialized
void patchPS1DRVConfigProtokernel();
#else
// HDD-OSD patches

// Patches newer IOP modules into HDD-OSD
void patchHOSDModules();

// Adds support for hiding partitions that have the '.HIDDEN' suffix
void patchBrowserHiddenPartitions();
#endif

#endif
