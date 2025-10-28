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
  opts->titleID = NULL;
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

  opts->titleID = generateTitleID(opts->bootPath);

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

// Frees all paths and arguments in SystemCNFOptions.
// Does not free the SystemCNFOptions itself.
void freeSystemCNFOptions(SystemCNFOptions *opts) {
  if (opts->bootPath)
    free(opts->bootPath);

  if (opts->ioprpPath)
    free(opts->ioprpPath);

  if (opts->titleVersion)
    free(opts->titleVersion);

  if (opts->titleID)
    free(opts->titleID);

  if (opts->args) {
    for (int i = 0; i < opts->argCount; i++)
      free(opts->args[i]);
    free(opts->args);
  }
}

// Parses the SYSTEM.CNF file on CD/DVD into SystemCNFOptions
int parseDiscCNF(SystemCNFOptions *opts) {
  // Open SYSTEM.CNF
  int fd = open("cdrom0:\\SYSTEM.CNF;1", O_RDONLY);
  if (fd < 0) {
    // Apparently not all PS1 titles have SYSTEM.CNF
    // Try to guess the title ID from the disc PVD
    opts->titleID = generateTitleID("cdrom:");
    if (opts->titleID) {
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

  ExecType type = parseSystemCNF(file, opts);
  fclose(file);
  free(cnf);
  if (type < 0) {
    DPRINTF("Failed to parse SYSTEM.CNF: %d\n", type);
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

// Attempts to generate a title ID from path
char *generateTitleID(char *path) {
  if (!path)
    return NULL;

  char *titleID = calloc(sizeof(char), 12);
  if (!titleID)
    return NULL;
  char *valuePtr = NULL;

  // Handle cdrom paths
  if (!strncmp(path, "cdrom", 5)) {
    // Get the start of the executable path
    valuePtr = strchr(path, '\\');
    if (!valuePtr) {
      valuePtr = strchr(path, ':'); // PS1 CDs don't have \ in the path
      if (!valuePtr) {
        DPRINTF("Failed to parse the executable for the title ID\n");
        goto fallback;
      }
    }
    valuePtr++;

    // Do a basic sanity check.
    // Some PS1 titles might have an off-by-one error in executable name (SLPS_11.111 instead of SLPS_111.11)
    if ((strlen(valuePtr) >= 11) && (valuePtr[4] == '_') && ((valuePtr[7] == '.') || (valuePtr[8] == '.'))) {
      strncpy(titleID, valuePtr, 11);
      return titleID;
    } else {
      // Try to guess the title ID from the disc PVD
      const char *tID = getPS1GenericTitleID();
      if (tID) {
        DPRINTF("Guessed the title ID from disc PVD: %s\n", tID);
        strncpy(titleID, tID, 11);
      }
      return titleID;
    }
    // If title ID doesn't pass the check, proceed to ELF fallback
  }

  // Handle hdd0 path
  if ((!strncmp(path, "hdd0:", 5))) {
    valuePtr = &path[5];

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
            goto whitespace; // If not, jump to removing possible whitespace from the name
        }

        // Change the '-' to '_', insert a dot after the first three digits and terminate the string
        titleID[4] = '_';
        titleID[10] = titleID[9];
        titleID[9] = titleID[8];
        titleID[8] = '.';
        DPRINTF("Got the title ID from partition name: %s\n", titleID);
        return titleID;
      }
      goto whitespace;
    }
  }

fallback:
  DPRINTF("Extracting title ID from ELF name: %s\n", titleID);
  // Try to extract the ELF name
  char *ext = strstr(path, ".ELF");
  if (!ext)
    ext = strstr(path, ".elf");

  // Find the start of the ELF name
  char *elfName = strrchr(path, '/');
  if (!elfName)
    return NULL;

  // Advance to point to the actual name
  elfName++;
  // Temporarily terminate the string at extension,
  // copy the first 11 characters and restore the '.'
  if (ext)
    *ext = '\0';

  strncpy(titleID, elfName, 11);

  if (ext)
    *ext = '.';

whitespace:
  // Remove whitespace at the end
  for (int i = 10; i >= 0; i--) {
    if (isspace((int)titleID[i])) {
      titleID[i] = '\0';
      break;
    }
  }
  DPRINTF("Path: %s\nTitle ID: %s\n", path, titleID);
  return titleID;
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
