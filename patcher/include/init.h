#ifndef _INIT_H_
#define _INIT_H_

#define USER_MEM_START_ADDR 0x100000
#define USER_MEM_END_ADDR 0x2000000

// Loads IOP modules
int initModules(void);

// Resets IOP before loading OSDSYS
void resetModules();

#endif
