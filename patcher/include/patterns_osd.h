#ifndef _PATTERNS_OSD_H_
#define _PATTERNS_OSD_H_
// OSDMenu OSD patterns
#include <stdint.h>

// Pattern for overriding OSD region
static uint32_t patternOSDRegion[] = {
    0x80020000, // lw   v0,0x0000,? <- patch target
    0x04410020, // bgez v0,0x002?
    0xffb00010, // sd   s0,0x0010,sp
};
static uint32_t patternOSDRegion_mask[] = {0xf00f0000, 0xfffffff0, 0xffffffff};

#endif
