#ifndef _INIT_H_
#define _INIT_H_

// Loads IOP modules
int initModules(void);

// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail();

#endif
