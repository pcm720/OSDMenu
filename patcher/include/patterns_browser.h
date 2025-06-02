#ifndef _PATTERNS_BROWSER_H_
#define _PATTERNS_BROWSER_H_
// OSDMenu browser launch patch patterns
#include <stdint.h>

#ifndef HOSD
// OSDMenu patterns

// Pattern for getting the address of the function that sets up exit to the clock screen
// There might be two functions matching this pattern, but the target function seems to be
// always the first one across all ROM versions.
// Target function should have 0x2402ffff (addiu v0,0xffff) within the first 10 to 20 instructions
static uint32_t patternSetupExitToPreviousModule[] = {
    0x27bdffe0, // addiu sp,sp,-0x20
    0xffb00000, // sd    s0,0x0000,sp
    0x0080802d, // daddu s0,a0,zero
    0xffbf0010, // sd    ra,0x0010,sp
    0x0c000000, // jal   ???
    0x24040002, // addiu a0,zero,0x0002
};
static uint32_t patternSetupExitToPreviousModule_mask[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff,
};

// Pattern for geting the address of the exit from browser function
static uint32_t patternExitToPreviousModule[] = {
    0x8f820000, // lw  v0,????,gp
    0x1440ff00, // bne v0,zero,0xff??
    0x00000000, // nop
    0x0c000000, // jal exitToPreviousModule
};
static uint32_t patternExitToPreviousModule_mask[] = {0xffff0000, 0xffffff00, 0xffffffff, 0xfc000000};

// Pattern for getting the address of the first or second browserDirSubmenuSetup call in browser main level handler
// There might be two browserDirSubmenuSetup calls in total on newer ROMs
// Another browserDirSubmenuSetup call is located within the last 0x150 bytes before this pattern.
static uint32_t patternBrowserMainLevelHandler[] = {
  0x24050001, // addiu a1,zero,0x0001
  0x0c000000, // jal browserDirSubmenuSetup
  0x00000000, // nop
  0x24045200, // addiu a0,zero,0x5200
};
static uint32_t patternBrowserMainLevelHandler_mask[] = {0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff};

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
