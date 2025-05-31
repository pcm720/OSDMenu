#ifndef _LOADER_H_
#define _LOADER_H_

#include <stdint.h>

// Embedded launcher
extern uint8_t launcher_elf[];
extern int size_launcher_elf;
// By default, points to launcher_elf
// Relocated by the HDD OSD patcher to USER_MEM_START_ADDR
extern uint8_t *launcher_elf_addr;

// Executes the payload by passing it to the launcher
void launchPayload(char *payloadPath);

// Uses the launcher to execute selected item
void launchItem(char *item);

// Uses the launcher to run the disc
void launchDisc();

#endif
