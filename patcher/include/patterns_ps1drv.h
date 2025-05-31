#ifndef _PATTERNS_PS1DRV_H_
#define _PATTERNS_PS1DRV_H_
// OSDMenu PS1DRV patch patterns
#include <stdint.h>

// Pattern for getting the address of OSD config init function
static uint32_t patternSetOSDConfig[] = {
    0x00000000, // nop
    0x0c000000, // jal ?
    0x0040202d, // daddu a0, v0, zero
    0x0c000000, // jal setOSDConfig <- target function
    0x00000000, // nop
    0x0c000000, // jal ?
};
static uint32_t patternSetOSDConfig_mask[] = {0xffffffff, 0xfc000000, 0xffffffff, 0xfc000000, 0xffffffff, 0xfc000000};
#endif
