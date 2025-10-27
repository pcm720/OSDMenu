#include "cnf.h"
#include "common.h"
#include "config.h"
#include "disc.h"
#include "dprintf.h"
#include "game_id.h"
#include "loader.h"
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
  SystemCNFOptions opts = {0};
  parseSystemCNF(file, &opts);
  fclose(file);

  DPRINTF("====\nSYSTEM.CNF:\n");
  if (opts.bootPath)
    DPRINTF("Boot path: %s\n", opts.bootPath);
  if (opts.titleID)
    DPRINTF("Title ID: %s\n", opts.titleID);
  if (opts.ioprpPath)
    DPRINTF("IOPRP Path: %s\n", opts.ioprpPath);
  DPRINTF("====\nSYSTEM.CNF:\n");
  DPRINTF("DEV9 Shutdown Type: %d\nSkip argv[0] = %d\nAdditional arguments:\n", opts.dev9ShutdownType, opts.skipArgv0);
  for (int i = 0; i < opts.argCount; i++)
    DPRINTF("argv[%d]: %s\n", i, opts.args[i]);
  DPRINTF("====\n");

  if (!opts.bootPath || opts.bootPath[0] == '\0' || !strcmp(opts.bootPath, "NOBOOT") || (opts.ioprpPath && !strcmp(opts.ioprpPath, "NOBOOT"))) {
    fioClose(fd);
    bootFail("Invalid boot path\n");
  }

  // Parse the options
  LoadOptions lopts = {0};
  lopts.skipArgv0 = opts.skipArgv0;
  lopts.dev9ShutdownType = opts.dev9ShutdownType;

  // IOPRP
  if (opts.ioprpPath && opts.ioprpPath[0] != '\0') {
    if (strcmp(opts.ioprpPath, "PATINFO"))
      lopts.ioprpPath = opts.ioprpPath;
    else {
      // Load the IOPRP image into memory
      if (readPATINFOFile((void *)PATINFO_IOPRP_MEM_ADDR, fd, header.ioprp, NULL)) {
        fioClose(fd);
        bootFail("Failed to read IOPRP image into memory\n");
      }

      lopts.ioprpMem = (void *)PATINFO_IOPRP_MEM_ADDR;
      lopts.ioprpSize = header.ioprp.size;
    }
  }

  // ELF
  if (!strncmp(opts.bootPath, "cdrom", 5)) {
    fioClose(fd);
    if (opts.titleID) {
      updateLaunchHistory(opts.titleID);
      free(opts.titleID);
    }
    handlePS2Disc(opts.bootPath);
  }

  lopts.argc = argc + opts.argCount;
  if (lopts.argc < 1)
    lopts.argc = 1;

  lopts.argv = malloc(lopts.argc * sizeof(char *));

  // Copy additional arguments while leaving argv[0] empty
  if (lopts.argc > 1) {
    for (int i = 1; i < argc; i++)
      lopts.argv[i] = argv[i];
    for (int i = 0; i < opts.argCount; i++)
      lopts.argv[i + argc] = opts.args[i];
  }

  if (!strcmp(opts.bootPath, "PATINFO")) {
    // Load ELF into memory
    uint32_t decryptAddr = 0;
    if (readPATINFOFile((void *)PATINFO_ELF_MEM_ADDR, fd, header.elf, &decryptAddr)) {
      fioClose(fd);
      bootFail("Failed to read the ELF file into memory\n");
    }

    if (decryptAddr != 0) {
      lopts.elfMem = (void *)decryptAddr;
    } else
      lopts.elfMem = (void *)PATINFO_ELF_MEM_ADDR;

    lopts.elfSize = header.elf.size;
    lopts.argv[0] = argv[0]; // Set partition path as argv[0]
  } else if (!strncmp(opts.bootPath, "pfs", 3)) {
    // Build the full path
    char *pfsPath = strchr(opts.bootPath, '/');
    char *fullPath = malloc(CNF_MAX_STR);
    fullPath[0] = '\0';
    if (pfsPath) {
      sprintf(fullPath, "%s:pfs:%s", argv[2], pfsPath);
      lopts.argv[0] = fullPath;
      free(opts.bootPath);
    }
  } else
    lopts.argv[0] = opts.bootPath; // Pass the opts.bootPath as-is

  fioClose(fd);
  if (opts.titleID) {
    updateLaunchHistory(opts.titleID);
    free(opts.titleID);
  }
  return loadELF(&lopts);
}
