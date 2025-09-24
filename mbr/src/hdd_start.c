#include "common.h"
#include "config.h"
#include "disc.h"
#include "dprintf.h"
#include "game_id.h"
#include "history.h"
#include "loader.h"
#include <cdrom.h>
#include <ps2sdkapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEWLIB_PORT_AWARE
#include <fileio.h>
#include <io_common.h>

#include <iopcontrol.h>
#include <libsecr.h>
#include <sifrpc.h>

// Attribute area header structs
#define AA_HEADER_MAGIC "PS2ICON3D"

typedef struct {
  uint32_t offset; // Attribute area offset
  uint32_t size;   // File size
} AttributeAreaFile;

typedef struct {
  char magic[9];
  uint8_t unused[3];
  uint32_t version;
  // Required files
  AttributeAreaFile systemCNF;
  AttributeAreaFile iconSys;
  AttributeAreaFile listIcon;
  AttributeAreaFile deleteIcon;
  // Optional files
  AttributeAreaFile elf;
  AttributeAreaFile ioprp;
} AttributeAreaHeader;

// Attempts to parse the title from the full boot path and update the history file
void updateLaunchHistory(char *bootPath);

// Loads the ELF file from the partition attribute area
int handlePATINFO(int partFd, AttributeAreaFile elf, int argc, char *argv[]) {
  DPRINTF("PATINFO: seeking to ELF location\n");
  int res = fioLseek(partFd, elf.offset, FIO_SEEK_SET);
  if (res < 0) {
    fioClose(partFd);
    DPRINTF("Failed to lseek: %d\n", res);
    return res;
  }

  // Allocate the memory ensuring the buffer is 64-byte aligned and is multiple of 512 bytes
  int bufSize = elf.size;
  if (bufSize % 0x200)
    bufSize = (bufSize + 0x200) & ~0x1FF;

  uint8_t *elfMem = (uint8_t *)0x1000000;
  DPRINTF("PATINFO: reading ELF into memory buffer of size %x @ %p\n", bufSize, elfMem);
  if (!elfMem) {
    fioClose(partFd);
    DPRINTF("Failed to allocate %x bytes\n", bufSize);
    return -ENOMEM;
  }

  res = fioRead(partFd, elfMem, bufSize);
  fioClose(partFd);
  if (res < elf.size) {
    DPRINTF("Failed to read ELF data: %x\n", res);
    return res;
  }

  if (*(uint32_t *)(&elfMem[0]) != 0x464c457f) {
    DPRINTF("PATINFO: trying to decrypt KELF using SECRMAN\n");
    if (!(res = SecrInit())) {
      DPRINTF("Failed to init libsecr: %x\n", res);
      return res;
    }
    elfMem = SecrDiskBootFile(elfMem);
    if (!elfMem) {
      DPRINTF("Failed to decrypt\n");
      return -1;
    }
    SecrDeinit();
  }

  // TODO: pass this to the embedded loader instead:
  // mem:<address>:<size> as argv[0]?
  DPRINTF("Loaded to %p\n", elfMem);
  for (int i = 0; i < 100; i++)
    DPRINTF("%02x ", elfMem[i]);

  DPRINTF("\n");

  DPRINTF("Rebooting IOP\n");
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };
  sceSifInitRpc(0);

  char *nargv[1] = {argv[2]};
  LoadOptions opts = {
      .elfMem = elfMem,
      .elfSize = elf.size,
      .argc = 1,
      .argv = nargv,
  };
  return loadELF(&opts);
}

// Starts HDD application using data from the partition attribute area header
int startHDDApplication(int argc, char *argv[]) {
  // Terminate the path at partition name
  char *v;
  if ((strlen(argv[2]) > 5) && (v = strchr(&argv[2][5], ':')))
    *v = '\0';

  // Using fileio functions here because fileXio can't seem to read the extended attribute area
  int fd = fioOpen(argv[2], FIO_O_RDONLY);
  if (fd < 0) {
    DPRINTF("Failed to open %s: %d\n", argv[2], fd);
    return fd;
  }

  // Read the attribute area header
  uint8_t buf[512] = {0};
  int res = fioRead(fd, &buf, 512);
  if (res < 0x40) {
    fioClose(fd);
    DPRINTF("Failed to read ELF data: %d\n", res);
    return res;
  }

  // Make sure the data read is actually the header by checking the magic
  if (memcmp(buf, AA_HEADER_MAGIC, 9))
    bootFail("Failed to read the partition attribute area header\n");

  // Copy raw data to reuse the buffer later
  AttributeAreaHeader header;
  memcpy(&header, buf, sizeof(AttributeAreaHeader));

  DPRINTF("Seeking to CNF location\n");
  res = fioLseek(fd, header.systemCNF.offset, FIO_SEEK_SET);
  if (res < 0) {
    fioClose(fd);
    DPRINTF("Failed to lseek: %d\n", res);
    return res;
  }

  DPRINTF("Reading CNF into memory\n");
  res = fioRead(fd, buf, 512);
  if (res < header.systemCNF.size) {
    DPRINTF("Failed to read CNF: %x\n", res);
    return res;
  }

  // Open memory buffer as stream
  FILE *file = fmemopen(buf, header.systemCNF.size, "r");
  if (file == NULL) {
    DPRINTF("Failed to open SYSTEM.CNF for reading\n");
    return -ENOENT;
  }

  // TODO: check whether we need to handle IOPRP arguments
  char bootPath[CNF_MAX_STR];
  char dev9Power[CNF_MAX_STR];
  char ioprpPath[CNF_MAX_STR];
  ExecType eType = parseSystemCNF(file, bootPath, NULL, dev9Power, ioprpPath);
  fclose(file);
  if (eType == ExecType_Error)
    DPRINTF("Failed to parse SYSTEM.CNF\n");

  printf("bootPath: %s\ndev9Power: %s\nioprpPath: %s\n", bootPath, dev9Power, ioprpPath);

  if (!strncmp(bootPath, "cdrom", 5)) {
    updateLaunchHistory(bootPath);
    handlePS2Disc(bootPath);
  }

  updateLaunchHistory(argv[2]);
  if (!strcmp(bootPath, "PATINFO")) {
    return handlePATINFO(fd, header.elf, argc, argv);
  }

  // TODO: finish PFS handling
  if (!strncmp(bootPath, "pfs", 3)) {
    // Build the full path
    sprintf("%s:%s", bootPath, argv[2], bootPath);
    LoadOptions opts = {
        .argc = argc,
        .argv = argv,
    };
    return loadELF(&opts);
  }

  return -1;
}

// Starts the dnasload applcation
void startDNAS(int argc, char *argv[]) {
  // The PSBBN MBR code runs "hdd0:__system:pfs:/dnas100/dnasload.elf"
  msg("\tdnasload arguments are not supported yet!\n\tPlease create a new issue in the OSDMenu repository with the following:\n\t\targc: %d\n", argc);
  for (int i = 0; i < argc; i++)
    msg("\t\targ[%d]: %s\n", i, argv[i]);

  msg("\n");

  bootFail("DNAS: argument not supported\n");
}

// Attempts to parse the title from the full boot path and update the history file
// Handles the following paths:
// 1. hdd0:?P.<PS2 title ID>[:.]
// 3. cdrom0:<executable name>;1
void updateLaunchHistory(char *bootPath) {
  char titleID[12] = {0};

  char *valuePtr = strchr(bootPath, '\\');
  if (valuePtr) {
    // Handle cdrom0 path
    valuePtr++;
    // Do a basic sanity check.
    if ((strlen(valuePtr) > 11) && (valuePtr[4] == '_') && (valuePtr[8] == '.')) {
      strncpy(titleID, valuePtr, 11);
      goto update;
    }
  }

  if ((!strncmp(bootPath, "hdd0:", 5))) {
    // Handle hdd0 path
    valuePtr = &bootPath[5];

    // Make sure we have a valid partition name
    if (valuePtr[1] == 'P' && valuePtr[2] == '.') {
      // Copy everything after ?P.
      valuePtr += 3;
      strncpy(titleID, valuePtr, 11);

      // Terminate the string at '.' or ':'
      if ((valuePtr = strchr(titleID, '.')) || (valuePtr = strchr(titleID, ':')))
        *valuePtr = '\0';

      // Check if this is a valid PS2 title ID
      if (titleID[4] == '-') {
        // Make sure all five characters after the '-' are digits
        for (int i = 5; i < 10; i++) {
          if ((titleID[i] < '0') || (titleID[i] > '9'))
            goto fail;
        }

        // Change the '-' to '_', insert a dot after the first three digits and terminate the string
        titleID[4] = '_';
        titleID[10] = titleID[9];
        titleID[9] = titleID[8];
        titleID[8] = '.';
        goto update;
      }
    }
  }

fail:
  DPRINTF("'%s': failed to format the title ID\n", bootPath);
  return;

update:
  DPRINTF("Title ID is %s\n", titleID);
  updateHistoryFile(titleID);
  if (!(settings.flags & FLAG_DISABLE_GAMEID))
    gsDisplayGameID(titleID);
  return;
}
