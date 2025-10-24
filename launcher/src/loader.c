#include "common.h"
#include "dprintf.h"
#include "game_id.h"
#include <ctype.h>
#include <kernel.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Loader ELF variables
extern uint8_t loader_elf[];
extern int size_loader_elf;

int LoadEmbeddedELF(uint8_t *boot_elf, int argc, char *argv[]);

// Attempts to generate a title ID from path
void extractELFName(char *path, char *dst) {
  // Try to extract the ELF name
  char *ext = strstr(path, ".ELF");
  if (!ext)
    ext = strstr(path, ".elf");

  // Find the start of the ELF name
  char *elfName = strrchr(path, '/');
  if (!elfName)
    return;

  // Advance to point to the actual name
  elfName++;
  // Temporarily terminate the string at extension,
  // copy the first 11 characters and restore the '.'
  if (ext)
    *ext = '\0';

  strncpy(dst, elfName, 11);

  if (ext)
    *ext = '.';

  // Remove whitespace at the end
  for (int i = 10; i >= 0; i--) {
    if (isspace((int)dst[i])) {
      dst[i] = '\0';
      break;
    }
  }
}

int LoadELFFromFile(int argc, char *argv[]) {
  if (settings.flags & FLAG_APP_GAMEID) {
    char titleID[12] = {0};
    if (titleID[0] == '\0')
      extractELFName(argv[0], titleID);

    DPRINTF("Title ID is %s\n", titleID);
    if (titleID[0] != '\0')
      gsDisplayGameID(titleID);
  }

  char **nargv = argv;
  // Process global arguments
  if (settings.gsmArgument) {
    // Add eGSM argument
    nargv = malloc((argc + 2) * sizeof(char *));
    for (int i = 0; i < argc; i++)
      nargv[i] = argv[i];

    argc += 2;
    nargv[argc - 1] = "-la=G";
    nargv[argc - 2] = settings.gsmArgument;
  }

  DPRINTF("Starting the embedded ELF loader with argc = %d\n", argc);
  for (int i = 0; i < argc; i++)
    DPRINTF("argv[%d] = %s\n", i, nargv[i]);

  return LoadEmbeddedELF(loader_elf, argc, nargv);
}

typedef struct {
  uint8_t ident[16]; // struct definition for ELF object header
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf_header_t;

typedef struct {
  uint32_t type; // struct definition for ELF program section header
  uint32_t offset;
  void *vaddr;
  uint32_t paddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  uint32_t align;
} elf_pheader_t;

// ELF-loading stuff
#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1

// Loads and executes the ELF boot_elf points to
// Based on PS2SDK elf-loader
int LoadEmbeddedELF(uint8_t *boot_elf, int argc, char *argv[]) {
  elf_header_t *eh;
  elf_pheader_t *eph;
  void *pdata;
  int i;

  // Wipe memory region where the ELF loader is going to be loaded (see loader/linkfile)
  memset((void *)0x00084000, 0, 0x00100000 - 0x00084000);

  eh = (elf_header_t *)boot_elf;
  if (_lw((uint32_t)&eh->ident) != ELF_MAGIC)
    __builtin_trap();

  eph = (elf_pheader_t *)(boot_elf + eh->phoff);

  // Scan through the ELF's program headers and copy them into RAM
  for (i = 0; i < eh->phnum; i++) {
    if (eph[i].type != ELF_PT_LOAD)
      continue;

    pdata = (void *)(boot_elf + eph[i].offset);
    memcpy(eph[i].vaddr, pdata, eph[i].filesz);
  }

  sceSifExitRpc();
  sceSifExitCmd();
  FlushCache(0);
  FlushCache(2);

  return ExecPS2((void *)eh->entry, NULL, argc, argv);
}
