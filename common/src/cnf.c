#include "cnf.h"
#include "dprintf.h"
#include "game_id_table.h"
#include <ctype.h>
#include <fcntl.h>
#include <kernel.h>
#include <libcdvd.h>
#include <osd_config.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Attempts to guess PS1 title ID from volume creation date stored in PVD
const char *getPS1GenericTitleID();

// Parses the SYSTEM.CNF file with support for OSDMenu PATINFO extensions
// Returns the executable type or a PIExecType_Error if an error occurs.
ExecType parseSystemCNF(FILE *cnfFile, SystemCNFOptions *opts) {
  opts->bootPath = NULL;
  opts->ioprpPath = NULL;
  opts->titleVersion = NULL;
  opts->args = NULL;
  opts->skipArgv0 = 0;
  opts->argCount = 0;
  opts->dev9ShutdownType = ShutdownType_All;

  char lineBuffer[255] = {0};
  char *valuePtr = NULL;
  linkedStr *args = NULL;
  ExecType type = ExecType_Error;
  while (fgets(lineBuffer, sizeof(lineBuffer), cnfFile)) { // fgets returns NULL if EOF or an error occurs
    // Find the start of the value
    valuePtr = strchr(lineBuffer, '=');
    if (!valuePtr)
      continue;

    // Trim whitespace and terminate the value
    do {
      valuePtr++;
    } while (isspace((int)*valuePtr));
    valuePtr[strcspn(valuePtr, "\r\n")] = '\0';

    if (!strncmp(lineBuffer, "BOOT2", 5)) { // PS2 title
      type = ExecType_PS2;
      opts->bootPath = strdup(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "BOOT", 4)) { // PS1 title
      type = ExecType_PS1;
      opts->bootPath = strdup(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "HDDUNITPOWER", 12)) { // DEV9 Power
      if (!strncmp(valuePtr, "NICHDD", 6))
        opts->dev9ShutdownType = ShutdownType_None;
      else if (!strncmp(valuePtr, "NIC", 3))
        opts->dev9ShutdownType = ShutdownType_HDD;
      continue;
    }
    if (!strncmp(lineBuffer, "IOPRP", 5)) { // IOPRP path
      opts->ioprpPath = strdup(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "VER", 3)) { // Title version
      opts->titleVersion = strdup(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "path", 4)) { // Extended launcher path
      type = ExecType_PS2;
      opts->bootPath = strdup(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "skip_argv0", 10)) { // Skip argv[0] flag
      opts->skipArgv0 = atoi(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "arg", 3)) { // Custom argument
      args = addStr(args, valuePtr);
      opts->argCount++;
      continue;
    }
  }

  if (opts->argCount > 0) {
    // Assemble argv array
    opts->args = malloc(opts->argCount);
    linkedStr *narg = NULL;
    int argIdx = 0;
    while (args != NULL) {
      opts->args[argIdx++] = args->str;
      narg = args->next;
      free(args);
      args = narg;
    }
  }
  return type;
}

void freeSystemCNFOptions(SystemCNFOptions *opts) {
  if (opts->bootPath)
    free(opts->bootPath);

  if (opts->ioprpPath)
    free(opts->ioprpPath);

  if (opts->titleVersion)
    free(opts->titleVersion);

  if (opts->args) {
    for (int i = 0; i < opts->argCount; i++)
      free(opts->args[i]);
    free(opts->args);
  }
}

// Parses the SYSTEM.CNF file on CD/DVD into passed string pointers and attempts to detect the title ID
// - bootPath (BOOT/BOOT2): boot path
// - titleID: detected title ID
// - titleVersion (VER): title version (optional, argument can be NULL)
//
// Returns the executable type or a negative number if an error occurs.
// Except for titleID (max 12 bytes), all other parameters must have CNF_MAX_STR bytes allocated.
int parseDiscCNF(char *bootPath, char *titleID, char *titleVersion) {
  // Open SYSTEM.CNF
  int fd = open("cdrom0:\\SYSTEM.CNF;1", O_RDONLY);
  if (fd < 0) {
    // Apparently not all PS1 titles have SYSTEM.CNF
    // Try to guess the title ID from the disc PVD
    const char *tID = getPS1GenericTitleID();
    if (tID) {
      DPRINTF("Guessed the title ID from disc PVD: %s\n", tID);
      strncpy(titleID, tID, 11);
      return ExecType_PS1;
    }
    return -ENOENT;
  }

  // Get the file size
  int size = lseek(fd, 0, SEEK_END);
  if (size <= 0) {
    DPRINTF("CDROM ERROR: Bad SYSTEM.CNF size\n");
    close(fd);
    return -EIO;
  }
  lseek(fd, 0, SEEK_SET);

  // Read file into memory
  char *cnf = malloc(size * sizeof(char));
  if (read(fd, cnf, size) != size) {
    DPRINTF("CDROM ERROR: Failed to read SYSTEM.CNF\n");
    close(fd);
    free(cnf);
    return -EIO;
  }
  close(fd);

  // Open memory buffer as stream
  FILE *file = fmemopen(cnf, size, "r");
  if (file == NULL) {
    DPRINTF("CDROM ERROR: Failed to open SYSTEM.CNF for reading\n");
    free(cnf);
    return -ENOENT;
  }

  SystemCNFOptions opts = {0};
  ExecType type = parseSystemCNF(file, &opts);
  fclose(file);
  free(cnf);
  if (type < 0) {
    freeSystemCNFOptions(&opts);
    DPRINTF("Failed to parse SYSTEM.CNF: %d\n", type);
    return type;
  }

  strcpy(bootPath, opts.bootPath);
  strcpy(titleVersion, opts.titleVersion);
  freeSystemCNFOptions(&opts);

  // Get the start of the executable path
  char *valuePtr = strchr(bootPath, '\\');
  if (!valuePtr) {
    valuePtr = strchr(bootPath, ':'); // PS1 CDs don't have \ in the path
    if (!valuePtr) {
      DPRINTF("CDROM: Failed to parse the executable for the title ID\n");
      return type;
    }
  }
  valuePtr++;

  // Do a basic sanity check.
  // Some PS1 titles might have an off-by-one error in executable name (SLPS_11.111 instead of SLPS_111.11)
  if ((strlen(valuePtr) > 11) && (valuePtr[4] == '_') && ((valuePtr[7] == '.') || (valuePtr[8] == '.')))
    strncpy(titleID, valuePtr, 11);
  else {
    // Try to guess the title ID from the disc PVD
    const char *tID = getPS1GenericTitleID();
    if (tID) {
      DPRINTF("Guessed the title ID from disc PVD: %s\n", tID);
      strncpy(titleID, tID, 11);
    }
  }

  return type;
}

// Attempts to guess PS1 title ID from volume creation date stored in PVD
const char *getPS1GenericTitleID() {
  char sectorData[2048] = {0};

  sceCdRMode mode = {
      .trycount = 3,
      .spindlctrl = SCECdSpinNom,
      .datapattern = SCECdSecS2048,
  };

  // Read sector 16 (Primary Volume Descriptor)
  if (!sceCdRead(16, 1, &sectorData, &mode) || sceCdSync(0)) {
    DPRINTF("Failed to read PVD\n");
    return NULL;
  }

  // Make sure the PVD is valid
  if (strncmp(&sectorData[1], "CD001", 5)) {
    DPRINTF("Invalid PVD\n");
    return NULL;
  }

  // Try to match the volume creation date at offset 0x32D against the table
  for (size_t i = 0; i < sizeof(gameIDTable) / sizeof(gameIDTable[0]); ++i) {
    if (strncmp(&sectorData[0x32D], gameIDTable[i].volumeTimestamp, 16) == 0) {
      return gameIDTable[i].gameID;
    }
  }
  return NULL;
}

// Adds a new string to linkedStr and returns
linkedStr *addStr(linkedStr *lstr, char *str) {
  linkedStr *newLstr = malloc(sizeof(linkedStr));
  if (!newLstr)
    return NULL;

  newLstr->str = strdup(str);
  newLstr->next = NULL;

  if (lstr) {
    linkedStr *tLstr = lstr;
    // If lstr is not null, go to the last element and
    // link the new element
    while (tLstr->next)
      tLstr = tLstr->next;

    tLstr->next = newLstr;
    return lstr;
  }

  // Else, return the new element as the first one
  return newLstr;
}

// Frees all elements of linkedStr
void freeLinkedStr(linkedStr *lstr) {
  if (!lstr)
    return;

  linkedStr *tPtr = lstr;
  while (lstr->next) {
    free(lstr->str);
    tPtr = lstr->next;
    free(lstr);
    lstr = tPtr;
  }
  free(lstr->str);
  free(lstr);
}
