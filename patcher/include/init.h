#ifndef _INIT_H_
#define _INIT_H_

#define USER_MEM_START_ADDR 0x100000
#define USER_MEM_END_ADDR 0x2000000
#define EXTRA_SECTION_START 0x1200000
#define EXTRA_SECTION_END 0x1300000

// Loads IOP modules
int initModules();

// Resets IOP before loading OSDSYS
void resetModules();

#ifdef HOSD
// Inits SIF RPC and fileXio without rebooting the IOP. Assumes all modules are already loaded
void shortInit();
#endif

#endif
