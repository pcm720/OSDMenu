#include "loader.h"
#include "dprintf.h"
#include <errno.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Based on PS2SDK elf-loader
extern uint8_t loader_elf[];
extern int size_loader_elf;

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

static char loaderArg[11] = "-la=\0\0\0\0\0\0\0";
static char elfMemArg[22] = {0};
static char ioprpMemArg[22] = {0};

// Loads and executes the embedded loader with provided arguments
int executeLoader(int argc, char *argv[]) {
  elf_header_t *eh;
  elf_pheader_t *eph;
  void *pdata;
  int i;

  DPRINTF("Starting the embedded ELF loader with argc = %d\n", argc);
  for (i = 0; i < argc; i++)
    DPRINTF("argv[%d] = %s\n", i, argv[i]);

  eh = (elf_header_t *)loader_elf;
  if (_lw((uint32_t)&eh->ident) != ELF_MAGIC)
    __builtin_trap();

  eph = (elf_pheader_t *)(loader_elf + eh->phoff);

  // Scan through the ELF's program headers and copy them into RAM
  for (i = 0; i < eh->phnum; i++) {
    if (eph[i].type != ELF_PT_LOAD)
      continue;

    pdata = (void *)(loader_elf + eph[i].offset);
    memcpy(eph[i].vaddr, pdata, eph[i].filesz);
  }

  FlushCache(0);
  FlushCache(2);

  return ExecPS2((void *)eh->entry, NULL, argc, argv);
}

// Loads ELF from file specified in argv[0]
int loadELF(LoadOptions *options) {
  int argPos = 4; // Account for "-la="
  int argc = 0;

  // Basic arguments
  if (options->resetIOP) {
    loaderArg[argPos++] = 'R';
    argc = 1;
  }
  if (options->skipArgv0) {
    loaderArg[argPos++] = 'A';
    argc = 1;
  }
  if (options->dev9ShutdownType == ShutdownType_HDD) {
    loaderArg[argPos++] = 'N';
    argc = 1;
  } else if (options->dev9ShutdownType == ShutdownType_None) {
    loaderArg[argPos++] = 'D';
    argc = 1;
  }

  // IOPRP
  if (options->ioprpMem || options->ioprpPath) {
    loaderArg[argPos++] = 'I';
    argc = (argc == 0) ? 2 : argc + 1;

    if (options->ioprpMem)
      sprintf(ioprpMemArg, "mem:%08lX:%08lX", (uint32_t)options->ioprpMem, (uint32_t)options->ioprpSize);
  }

  // In-memory ELF
  if (options->elfMem) {
    loaderArg[argPos++] = 'E';
    argc = (argc == 0) ? 2 : argc + 1;

    sprintf(elfMemArg, "mem:%08lX:%08lX", (uint32_t)options->elfMem, (uint32_t)options->elfSize);
  }

  // eGSM
  if (options->eGSM) {
    loaderArg[argPos++] = 'G';
    argc = (argc == 0) ? 2 : argc + 1;
  }

  char **argv = options->argv;
  if (argc != 0) {
    argv = malloc((argc + options->argc) * sizeof(char *));
    // Copy the original arguments
    for (int i = 0; i < options->argc; i++)
      argv[i] = options->argv[i];

    // Add loader arguments (ELF, IOPRP, GSM argument order is important)
    int argvOffset = argc;
    argc += options->argc;

    // ELF argument
    if (elfMemArg[0] != '\0')
      argv[argc - (argvOffset--)] = elfMemArg;

    // IOPRP argument
    if (ioprpMemArg[0] != '\0')
      argv[argc - (argvOffset--)] = ioprpMemArg;
    else if (options->ioprpPath)
      argv[argc - (argvOffset--)] = options->ioprpPath;

    // GSM argument
    if (options->eGSM)
      argv[argc - (argvOffset--)] = options->eGSM;

    // Loader arguments
    argv[argc - argvOffset] = loaderArg;
  } else
    argc = options->argc;

  return executeLoader(argc, argv);
}
