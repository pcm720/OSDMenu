#ifndef _PATTERNS_GS_H_
#define _PATTERNS_GS_H_
// OSDMenu video mode patterns
#include <stdint.h>

// Pattern for overriding the sceGsPutDispEnv function call in sceGsSwapDBuff
static uint32_t patternGsPutDispEnv[] = {
    // Searching for particular pattern in sceGsSwapDBuff
    0x0c000000, // jal sceGsPutDispEnv
    0x00512021, // addu a0,v0,s1
    0x12000005, // beq s0,zero,0x05
    0x00000000, // nop
    0x0c000000, // jal sceGsPutDrawEnv
    0x26240140, // addiu a0,s1,0x0140
    0x10000004, // beq zero,zero,0x04
    0xdfbf0020, // ld ra,0x0020,sp
};
static uint32_t patternGsPutDispEnv_mask[] = {
    0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, //
    0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, //
};
#endif
