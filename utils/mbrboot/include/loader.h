#ifndef _LOADER_H_
#define _LOADER_H_
#include <stdint.h>

// Payload ELF variables
extern uint8_t payload_elf[];
extern int size_payload_elf;

// Loader ELF variables
extern uint8_t loader_elf[];
extern int size_loader_elf;

// Loads and executes the ELF boot_elf points to
int LoadEmbeddedELF(uint8_t *boot_elf, int argc, char *argv[]);

#endif
