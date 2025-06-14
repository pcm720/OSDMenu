#ifndef _LOADER_H_
#define _LOADER_H_
#include <stdint.h>

// Loads and executes ELF specified in argv[0]
int LoadELFFromFile(int argc, char *argv[]);

// Loads and executes the ELF boot_elf points to
int LoadEmbeddedELF(uint8_t *boot_elf, int argc, char *argv[]);

#endif
