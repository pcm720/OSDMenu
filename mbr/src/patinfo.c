#include "common.h"
#include "config.h"
#include "disc.h"
#include "dprintf.h"
#include "game_id.h"
#include "history.h"
#include "loader.h"
#include <cdrom.h>
#include <iopcontrol.h>
#include <libsecr.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEWLIB_PORT_AWARE
#include <fileio.h>
#include <io_common.h>

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

// Addresses for PATINFO files
#define PATINFO_ELF_MEM_ADDR 0x1000000
#define PATINFO_IOPRP_MEM_ADDR 0x1f00000

// Attempts to read the embedded file from the partition attribute area.
// If decryptAddr is not NULL, will attempt to decrypt the file and set the location of the decrypted ELF.
// decryptAddr will be set to 0 if decryption was not required.
int readPATINFOFile(void *dst, int partFd, AttributeAreaFile file, uint32_t *decryptAddr) {
  DPRINTF("PATINFO: seeking to file location\n");
  int res = fioLseek(partFd, file.offset, FIO_SEEK_SET);
  if (res < 0) {
    fioClose(partFd);
    DPRINTF("PATINFO: failed to lseek: %d\n", res);
    return res;
  }

  // Allocate the memory ensuring the buffer is 64-byte aligned and is multiple of 512 bytes
  int bufSize = file.size;
  if (bufSize % 0x200)
    bufSize = (bufSize + 0x200) & ~0x1FF;

  DPRINTF("PATINFO: reading ELF into memory buffer of size %x @ %p\n", bufSize, dst);
  res = fioRead(partFd, dst, bufSize);
  fioClose(partFd);
  if (res < file.size) {
    DPRINTF("PATINFO: failed to read the file data: %x\n", res);
    return res;
  }

  if (decryptAddr) {
    if (*(uint32_t *)dst != 0x464c457f) {
      DPRINTF("PATINFO: trying to decrypt the file using SECRMAN\n");
      if (!(res = SecrInit())) {
        msg("PATINFO: failed to init libsecr: %x\n", res);
        return res;
      }
      dst = SecrDiskBootFile(dst);
      if (!dst || (*(uint32_t *)dst != 0x464c457f)) {
        msg("PATINFO: failed to decrypt\n");
        return -1;
      }
      SecrDeinit();

      *decryptAddr = (uint32_t)dst;
    } else
      *decryptAddr = 0;
  }
  DPRINTF("PATINFO: loaded to %p\n", dst);
  return 0;
}

// Starts HDD application using data from the partition attribute area header
// Assumes argv[0] is the partition path
int startHDDApplication(int argc, char *argv[]) {
  // Terminate the path at partition name
  char *v;
  if ((strlen(argv[0]) > 5) && (v = strchr(&argv[0][5], ':')))
    *v = '\0';

  // Using fileio functions here because fileXio can't seem to read the extended attribute area
  int fd = fioOpen(argv[0], FIO_O_RDONLY);
  if (fd < 0) {
    DPRINTF("Failed to open %s: %d\n", argv[0], fd);
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
    fioClose(fd);
    bootFail("Failed to read SYSTEM.CNF\n");
  }

  // Open memory buffer as stream
  FILE *file = fmemopen(buf, header.systemCNF.size, "r");
  if (file == NULL) {
    fioClose(fd);
    bootFail("Failed to open SYSTEM.CNF for reading\n");
  }

  // Parse SYSTEM.CNF file from the attribute area
  char *bootPath = calloc(sizeof(char), CNF_MAX_STR);
  char *dev9Power = calloc(sizeof(char), CNF_MAX_STR);
  char *ioprpPath = calloc(sizeof(char), CNF_MAX_STR);
  ExecType eType = parseSystemCNF(file, bootPath, NULL, dev9Power, ioprpPath);
  fclose(file);
  if (eType == ExecType_Error) {
    fioClose(fd);
    msg("Failed to parse SYSTEM.CNF\n");
    return -EINVAL;
  }

  DPRINTF("====\nSYSTEM.CNF:\nBoot path: %s\nDEV9 power: %s\nIOPRP Path: %s\n====\n", bootPath, dev9Power, ioprpPath);

  if (bootPath[0] == '\0' || !strcmp(bootPath, "NOBOOT") || !strcmp(ioprpPath, "NOBOOT")) {
    fioClose(fd);
    bootFail("Invalid boot path\n");
  }

  // Parse the options
  LoadOptions opts = {0};

  // DEV9
  if (dev9Power[0] != '\0') {
    if (!strcmp(dev9Power, "NIC"))
      opts.dev9ShutdownType = ShutdownType_HDD;
    else if (!strcmp(dev9Power, "NICHDD"))
      opts.dev9ShutdownType = ShutdownType_None;
  }

  // IOPRP
  if (ioprpPath[0] != '\0') {
    if (strcmp(ioprpPath, "PATINFO"))
      opts.ioprpPath = ioprpPath;
    else {
      // Load the IOPRP image into memory
      if (readPATINFOFile((void *)PATINFO_IOPRP_MEM_ADDR, fd, header.ioprp, NULL)) {
        fioClose(fd);
        bootFail("Failed to read IOPRP image into memory\n");
      }

      opts.ioprpMem = (void *)PATINFO_IOPRP_MEM_ADDR;
      opts.ioprpSize = header.ioprp.size;
    }
  }

  // ELF
  if (!strncmp(bootPath, "cdrom", 5)) {
    fioClose(fd);
    updateLaunchHistory(bootPath);
    handlePS2Disc(bootPath);
  }

  opts.argc = argc - 2;
  if (opts.argc < 1)
    opts.argc = 1;

  opts.argv = malloc(opts.argc * sizeof(char *));

  // Copy additional arguments
  if (opts.argc > 1)
    for (int i = 1; i < opts.argc; i++)
      opts.argv[i] = argv[i];

  if (!strcmp(bootPath, "PATINFO")) {
    // Load ELF into memory
    uint32_t decryptAddr = 0;
    if (readPATINFOFile((void *)PATINFO_ELF_MEM_ADDR, fd, header.elf, &decryptAddr)) {
      fioClose(fd);
      bootFail("Failed to read the ELF file into memory\n");
    }

    if (decryptAddr != 0) {
      opts.elfMem = (void *)decryptAddr;
    } else
      opts.elfMem = (void *)PATINFO_ELF_MEM_ADDR;

    opts.elfSize = header.elf.size;
    opts.argv[0] = argv[0]; // Set partition path as argv[0]
  } else if (!strncmp(bootPath, "pfs", 3)) {
    // Build the full path, reusing dev9Power as the string buffer
    char *pfsPath = strchr(bootPath, '/');
    if (pfsPath) {
      sprintf(dev9Power, "%s:pfs:%s", argv[2], pfsPath);
      opts.argv[0] = dev9Power;
    }
  } else
    opts.argv[0] = bootPath; // Pass the bootPath as-is

  fioClose(fd);
  updateLaunchHistory(argv[0]);
  return loadELF(&opts);
}

// Starts the dnasload applcation
void startDNAS(int argc, char *argv[]) {
  // Update the history file
  updateLaunchHistory(argv[2]);

  // Override argv[1] with the dnasload path and adjust argc
  argv[1] = "hdd0:__system:pfs:/dnas100/dnasload.elf";
  argc -= 1;
  LoadOptions opts = {
      .argc = argc,
      .argv = &argv[1],
  };
  loadELF(&opts);
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

      if (settings.flags & FLAG_APP_GAMEID)
        // Display game ID even if the title ID is invalid
        goto gameid;
    }
  }

fail:
  DPRINTF("'%s': failed to format the title ID\n", bootPath);
  return;

update:
  DPRINTF("Title ID is %s\n", titleID);
  updateHistoryFile(titleID);
gameid:
  if (!(settings.flags & FLAG_DISABLE_GAMEID))
    gsDisplayGameID(titleID);
  return;
}
