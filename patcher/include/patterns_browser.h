#ifndef _PATTERNS_BROWSER_H_
#define _PATTERNS_BROWSER_H_
// OSDMenu browser launch patch patterns
#include <stdint.h>

#ifndef HOSD
// OSDMenu patterns

// Pattern for getting the address of the Browser file properties/Copy/Delete view init function
// Seems to be consistent across all ROM versions, including protokernels
static uint32_t patternBrowserFileMenuInit[] = {
    0x27bdffe0, // addiu sp,sp,-0x20
    0x30a500ff, // andi  a1,a1,0x00FF
                // 0xffb00000, // sd s0,0x0000,sp // s0 contains current memory card index
                // 0xffbf0010, // sd ra,0x0010,sp
                // 0x0c000000, // jal browserDirSubmenuInitView <- target function
};
static uint32_t patternBrowserFileMenuInit_mask[] = {0xffffffff, 0xffffffff};

// Pattern for getting the address of the currently selected memory card
// Located in browserDirSubmenuInitView function
static uint32_t patternBrowserSelectedMC[] = {
    0x8f820000, // lw v0,0x0000,gp <-- address relative to $gp
    0x2442fffe, // addiu v0,v0,-0x2
    0x2c420000, // sltiu v0,v0,0x?
};
static uint32_t patternBrowserSelectedMC_mask[] = {0xffff0000, 0xffffffff, 0xfffffff0};

// Pattern for getting the address of the function that
// triggers sceMcGetDir call and returns the directory size or -8 if result
// is yet to be retrieved.
// When this function returns -8, browserDirSubmenuInitView gets called repeatedly
// until browserGetMcDirSize returns valid directory size
static uint32_t patternBrowserGetMcDirSize[] = {
    0x0c000000, // jal browserGetMcDirSize <-- target function
    0x00000000, // nop
    0x0040802d, // daddu s0,v0,zero
    0x2402fff8, // addiu v0,zero,0x8
    0x12020030, // beq   s0,v0,0x003?
};
static uint32_t patternBrowserGetMcDirSize_mask[] = {0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xfffffff0};
#else
// HOSDMenu patterns

// Pattern for getting the address of the function related to filling the browser icon properties
static uint32_t patternBuildIconData[] = {
    0xacc00690, // sw zero,0x0690,a2
    0x0c000000, // jal buildIconData
    0xacc00694, // sw zero,0x0694,a2
};
static uint32_t patternBuildIconData_mask[] = {0xffffffff, 0xfc000000, 0xffffffff};

// Pattern for geting the address of the exit from browser function
static uint32_t patternExitToPreviousModule[] = {
    0x8f820000, // sw zero,0x0690,a2
    0x14000000, // bne ??, ??, ??
    0x00000000, // nop
    0x0c000000, // jal exitToPreviousModule
    0x00000000, // nop
};
static uint32_t patternExitToPreviousModule_mask[] = {0xffff0000, 0xff000000, 0xffffffff, 0xfc000000, 0xffffffff};

// Pattern for geting the address of the function that sets up data for main menu, used to launch CD/DVD or HDD applications
// When this function is called, $s2 register contains the address of the currently selected entry icon data
static uint32_t patternSetupExitToPreviousModule[] = {
    0x0c000000, // jal setupExitToPreviousModule
    0x8e000694, // lw zero,0x0694,??
};
static uint32_t patternSetupExitToPreviousModule_mask[] = {0xfc000000, 0xff00ffff};
#endif

#endif
