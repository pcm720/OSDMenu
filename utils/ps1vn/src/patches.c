#include "common.h"
#include <debug.h>
#include <kernel.h>
#include <stdint.h>

// Some macros used for patching
#define JAL(addr) (0x0c000000 | (0x3ffffff & ((addr) >> 2)))
#define JMP(addr) (0x08000000 | (0x3ffffff & ((addr) >> 2)))
#define GETJADDR(addr) ((addr & 0x03FFFFFF) << 2)

static uint64_t *emu_SYNCHV_I = (uint64_t *)0x70001920;
static uint64_t *emu_SYNCHV_NI = (uint64_t *)0x70001928;

static void patchROMVERRegion(void);
static int initVideoModeParams(void);
static uint32_t *scanForPattern(uint32_t *start, uint32_t range, const uint32_t *pattern, uint32_t PatternSize, const uint32_t *mask);

int hookUnpackFunction(void *start) {
  uint32_t *patchLoc;
  int result;
  const static uint32_t unpackFuncPattern[] = {
      // FlushCache(0); FlushCache(2);
      0x24030064, // addiu v1, zero, $0064
      0x0000202d, // daddu a0, zero, zero
      0x0000000c, // syscall (00000)
      0x24030064, // addiu v1, zero, $0064
      0x24040002, // addiu a0, zero, $0002
      0x0000000c  // syscall (00000)
  };
  const static uint32_t unpackFuncPatternMask[] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

  if ((patchLoc = scanForPattern(start, 0x1000, unpackFuncPattern, sizeof(unpackFuncPattern), unpackFuncPatternMask)) != NULL) {
    patchLoc[0] = 0x3c020000 | ((uint32_t)&patchROMVERRegion) >> 16;      // lui $v0, &patchROMVERRegion
    patchLoc[1] = 0x34420000 | (((uint32_t)&patchROMVERRegion) & 0xFFFF); // ori $v0, &patchROMVERRegion
    patchLoc[2] = 0x0040f809;                                             // jalr $v0
    patchLoc[3] = 0;                                                      // nop
    patchLoc[4] = 0;                                                      // nop
    patchLoc[5] = 0;                                                      // nop
    result = 0;

    FlushCache(0);
    FlushCache(2);
  } else {
    result = 1;
  }

  return result;
}

void patchPS1DRVVMode(void) {
  uint32_t *patchLoc, *retAddr;
  // For NTSC consoles.
  const static uint32_t vModeSetPatternNTSC[] = {
      0x24040000, // addiu a0, zero, selectedVMode
      0x00661825, // or v1, v1, a2
      0xaf840000, // sw a0, VideoModeParam(gp)
  };
  const static uint32_t vModeSetPatternMaskNTSC[] = {0xFFFF0000, 0xFFFFFFFF, 0xFFFF0000};

  // For PAL consoles.
  const static uint32_t vModeSetPatternPAL[] = {
      0x24030000, // addiu v1, zero, selectedVMode
      0x3c0400a9, // lui a0, $00a9
      0x34840005, // ori a0, a0, $0005
  };
  const static uint32_t vModeSetPatternMaskPAL[] = {0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF};
  const static uint32_t vModeSetRetPattern[] = {
      0x03e00008 // jr ra
  };
  const static uint32_t vModeSetRetPatternMask[] = {0xFFFFFFFF};
  const uint32_t *pattern, *mask;
  uint32_t PatternSize;

  if (consoleRegion == CONSOLE_REGION_EUROPE) {
    pattern = vModeSetPatternPAL;
    mask = vModeSetPatternMaskPAL;
    PatternSize = sizeof(vModeSetPatternPAL);
  } else {
    pattern = vModeSetPatternNTSC;
    mask = vModeSetPatternMaskNTSC;
    PatternSize = sizeof(vModeSetPatternNTSC);
  }

  if ((patchLoc = scanForPattern((void *)0x00200000, 0x20000, pattern, PatternSize, mask)) != NULL &&
      (retAddr = scanForPattern(patchLoc, 0x80, vModeSetRetPattern, sizeof(vModeSetRetPattern), vModeSetRetPatternMask)) != NULL) {
    *patchLoc = ((*patchLoc) & 0xFFFF0000) | (selectedVMode & 3);
    *retAddr = JMP((uint32_t)&initVideoModeParams);
  }

  FlushCache(0);
  FlushCache(2);
}

static void patchROMVERRegion(void) {
  const static uint32_t romverParserPattern[] = {
      0x93a20004, // lbu v0, $0004(sp)
      0x24030043, // addiu v1, zero, $0043
      0x00021600, // sll v0, v0, 24
      0x00022603  // sra v0, v0, 24
  };
  const static uint32_t romverParserMask[] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

  // Cause the new PS1DRV program to "see" the desired region, instead of the letter in ROMVER.
  // This will also change the compatibility list used.
  char region;
  switch (discRegion) {
  case DISC_REGION_USA:
    region = 'A';
    break;
  case DISC_REGION_EUROPE:
    region = 'E';
    break;
  default:
    region = 'J';
  }

  uint32_t *patchLoc;
  if ((patchLoc = scanForPattern((void *)0x00200000, 0x20000, romverParserPattern, sizeof(romverParserPattern), romverParserMask)) != NULL) {
    *patchLoc = 0x24020000 | region; // li $v0, region
  }

  FlushCache(0);
  FlushCache(2);
}

static int initVideoModeParams(void) { // Replacement function. Overrides the SYCHV parameters set by the driver.
  uint64_t temp = 0;

  if (selectedVMode == 3) {
    // PAL
    *emu_SYNCHV_I = 0x00A9000502101401;
    *emu_SYNCHV_NI = 0x00A9000502101404;
  } else {
    // NTSC
    temp = (temp & 0xFFFFFC00) | 3;
    temp = (temp & 0xFFF003FF) | 0x1800;
    temp = (temp & 0xC00FFFFF) | 0x01200000;
    temp = (temp & 0xFFFFFC00FFFFFFFF) | (0xC000LL << 19);
    temp = (temp & 0xFFE003FFFFFFFFFF) | (0xF300LL << 35);
    temp = (temp & 0x801FFFFFFFFFFFFF) | (0xC000LL << 40);
    *emu_SYNCHV_I = temp;

    // Nearly a repeat of the block above.
    temp = (temp & 0xFFFFFC00) | 4;
    temp = (temp & 0xFFF003FF) | 0x1800;
    temp = (temp & 0xC00FFFFF) | 0x01200000;
    temp = (temp & 0xFFFFFC00FFFFFFFF) | (0xC000LL << 19);
    temp = (temp & 0xFFE003FFFFFFFFFF) | (0xF300LL << 35);
    temp = (temp & 0x801FFFFFFFFFFFFF) | (0xC000LL << 40);
    *emu_SYNCHV_NI = temp;
  }

  return 0;
}

static uint32_t *scanForPattern(uint32_t *start, uint32_t range, const uint32_t *pattern, uint32_t PatternSize, const uint32_t *mask) {
  uint32_t i, *result, patternOffset;

  for (result = NULL, i = 0; i < range; i += 4) {
    for (patternOffset = 0; patternOffset < PatternSize; patternOffset += 4) {
      if ((start[(i + patternOffset) / 4] & mask[patternOffset / 4]) != pattern[patternOffset / 4])
        break;
    }

    if (patternOffset == PatternSize) { // Pattern found.
      result = &start[i / 4];
      break;
    }
  }

  return result;
}
