#ifndef _PATTERNS_VERSION_H_
#define _PATTERNS_VERSION_H_
// OSDMenu version patch patterns
#include <stdint.h>

#ifndef HOSD
// Pattern for injecting custom entries into the Version submenu
static uint32_t patternVersionInit[] = {
    // Search pattern in the OSD pad handler table (at address 0x208800 for ROMVER 0200E20040614):
    0x00000000, // nop
                // case 0x16:
    0x0c000000, // jal versionInfoInit — target function that inits version submenu strings
    0x3c10001f, // lui s0,0x001F
    0x1000ffad, // beq zero, zero, <switch>
    0x00000000, // nop
};
static uint32_t patternVersionInit_mask[] = {0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff};

// Pattern for getting the Version submenu string table address from the versionInfoInit function.
// Address will point to the first string location ("Console")
// The table seems to have a fixed location at address 0x1f1238 in ROM versions 1.20-2.50
static uint32_t patternVersionStringTable[] = {
    // Search pattern in the versionInfoInit function that points to the string table address
    0x3c030000, // lui  v1, <top address bytes>
    0x00000018, // mult ??,??,??
    0x34630000, // ori  v1,v1, <bottom address bytes>
};
static uint32_t patternVersionStringTable_mask[] = {0xffff0000, 0x00000018, 0xffff0000};

//
// Protokernel patterns
//

// Pattern for injecting custom entries into the Version submenu
static uint32_t patternVersionInit_Proto[] = {
    // <init Unit, Browser, CD Player, PS1DRV version>
    0x24a615c8, // addiu a2,a1,0x15C8
    0x24a41598, // addiu a0,a1,0x1598
    0x0c000000, // jal   getDVDPlayerVersion
};
static uint32_t patternVersionInit_Proto_mask[] = {0xffffffff, 0xffffffff, 0xfc000000};

// Pattern for getting the address of sceCdApplySCmd function
// After finding this pattern, go back until reaching
// _lw(addr) & 0xffff0000 == 0x27bd0000 (addiu $sp,$sp, ?) to get the function address.
static uint32_t patternCdApplySCmd_Proto[] = {
    //  0x27bd0000, // addiu $sp,$sp,??
    //  ~50 instructions
    0x26500000, // addiu s0,s2,??
    0x3c058000, // lui   a1,0x8000
    0x0200202d, // daddu a0,s0,zero
    0x34a50593, // ori   a1,a1,0x0593
    0x0c000000, // jal   sceSifBindRpc
    0x0000302d, // daddu a2,zero,zero
};
static uint32_t patternCdApplySCmd_Proto_mask[] = {0xffff0000, 0xffffffff, 0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff};
#else
// Pattern for injecting custom entries into the Version submenu
static uint32_t patternVersionInit[] = {
    0x0c000000, // jal   initVersionInfoStrings
    0x26300000, // addiu s0,s1,0x????
    0x10000003, // beq   zero,zero,0x0003
};
static uint32_t patternVersionInit_mask[] = {0xfc000000, 0xffff0000, 0xffffffff};

// Using fixed offset to avoid unneeded tracing
uint32_t hddosdVerinfoStringTableAddr = 0x1f1298;
#endif

// Pattern for getting the address of sceCdApplySCmd function
// After finding this pattern, go back until reaching
// _lw(addr) & 0xffff0000 == 0x27bd0000 (addiu $sp,$sp, ?) to get the function address.
static uint32_t patternCdApplySCmd[] = {
    //  0x27bd0000, // addiu $sp,$sp,??
    //  ~15-20 instructions
    0x0c000000, // jal WaitSema
    0x00000000, // ...
    0x3c000000, // lui v?, ?
    0x24000019, // li v?, 0x19
    0x00000000, // ...
    0x0c000000, // jal sceCdSyncS
};
static uint32_t patternCdApplySCmd_mask[] = {0xfc000000, 0x00000000, 0xfc000000, 0xfc00ffff, 0x00000000, 0xfc000000};

// Pattern for getting the address of the sceGsGetGParam function
static uint32_t patternGsGetGParam[] = {
    // Searching for particular pattern in sceGsResetGraph
    0x0c000000, // jal sceGsGetGParam
    0x00000000, // nop
    0x3c031200, // lui v1,0x1200 // REG_GS_CSR = 0x200
    0x24040200, // li a0,0x200
    0x34631000, // ori v1,v1,0x1000
};
static uint32_t patternGsGetGParam_mask[] = {0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
#endif
