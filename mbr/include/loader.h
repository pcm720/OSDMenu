#ifndef _LOADER_H_
#define _LOADER_H_
#include <stdint.h>

// Loads and executes the ELF boot_elf points to
// Based on PS2SDK elf-loader
int LoadEmbeddedELF(int resetIOP, uint8_t *boot_elf, int argc, char *argv[]);

// Loads ELF from file specified in argv[0]
int LoadELFFromFile(int resetIOP, int argc, char *argv[]);

#endif
