#include <kernel.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

// Loader ELF variables
extern uint8_t payload_elf[];
extern int size_payload_elf;
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

// Based on PS2SDK elf-loader
int main(int argc, char *argv[]) {
  elf_header_t *eh;
  elf_pheader_t *eph;
  void *pdata;
  int i;

  eh = (elf_header_t *)payload_elf;
  if (_lw((uint32_t)&eh->ident) != ELF_MAGIC)
    __builtin_trap();

  eph = (elf_pheader_t *)(payload_elf + eh->phoff);

  // Scan through the ELF's program headers and copy them into RAM
  for (i = 0; i < eh->phnum; i++) {
    if (eph[i].type != ELF_PT_LOAD)
      continue;

    pdata = (void *)(payload_elf + eph[i].offset);
    memcpy(eph[i].vaddr, pdata, eph[i].filesz);
  }

  SifExitRpc();
  FlushCache(0);
  FlushCache(2);

  return ExecPS2((void *)eh->entry, NULL, argc, argv);
}
