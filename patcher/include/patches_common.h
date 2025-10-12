#ifndef _PATCHES_COMMON_H_
#define _PATCHES_COMMON_H_
#include <stdint.h>

typedef struct {
  uint16_t sceGsInterMode; // Interlace/non-interlace value
  uint16_t sceGsOutMode;   // NTSC/PAL value
  uint16_t sceGsFFMode;    // FIELD/FRAME value
  uint16_t sceGsVersion;   // GS version
} sceGsGParam;

// Loads OSDSYS from ROM and handles the patching
void launchOSDSYS(int argc, char *argv[]);

// Calls OSDSYS deinit function
void deinitOSDSYS();

#ifndef HOSD
// All protokernel menu code seems to be located starting from 0x600000 (OSDSYS is loaded at 0x200000)
#define PROTOKERNEL_MENU_OFFSET 0x400000

// Loads OSDSYS from ROM and injects the patching function into OSDSYS
void launchProtokernelOSDSYS();
#endif

// Searches for byte pattern in memory
uint8_t *findPatternWithMask(uint8_t *buf, uint32_t bufsize, uint8_t *bytes, uint8_t *mask, uint32_t len);

// Searches for string in memory
char *findString(const char *string, char *buf, uint32_t bufsize);

#endif
