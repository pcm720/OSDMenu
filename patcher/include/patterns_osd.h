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

// Pattern for overriding disc handling in Clock thread
// In some ROMs, may appear twice within 0x100 bytes
static uint32_t patternOSDClockLaunchDisc[] = {
    0x24020001, // li v0,0x1
    0xac620004, // sw v0,0x4(v1) <- patch target (nop)
};
static uint32_t patternOSDClockLaunchDisc_mask[] = {0xffffffff, 0xffffffff};

#ifndef HOSD
// Pattern for overriding disc handling in SCE intro
// In some ROMs, may appear twice within 0x200 bytes
static uint32_t patternOSDIntroLaunchDisc[] = {
    0x8c030000, // lw v1,0x??,?
    0x2464ff00, // addiu a0,v1,-0x??
};
static uint32_t patternOSDIntroLaunchDisc_mask[] = {0xff0fff00, 0xffffff00};
#else
// Pattern for overriding disc handling in HDD-OSD main()
// It reads CDVD disc type register directly (0x1f40200f), so these branch instructions need to be patched out completely
static uint32_t patternOSDIntroLaunchDisc[] = {
    0x306300ff, // andi v1,v1,0xff
    0x10640022, // beq  v1,a0,0x0022
    0x28620015, // slti v0,v1,0x0015
    0x10400007, // beq  v1,a0,0x0007
    0x28620012, // slti v0,v1,0x0012
    0x10400019, // beq  v1,a0,0x0019
    0x28620010, // slti v0,v1,0x0010
    0x10400012, // beq  v1,a0,0x0012
};
static uint32_t patternOSDIntroLaunchDisc_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
#endif

#endif
