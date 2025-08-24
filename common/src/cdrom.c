#include "cdrom.h"
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

// Parses SYSTEM.CNF on disc into bootPath, titleID and titleVersion
// Returns disc type or a negative number if an error occurs
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
      return DiscType_PS1;
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

  char lineBuffer[255] = {0};
  char *valuePtr = NULL;
  DiscType type = -1;
  while (fgets(lineBuffer, sizeof(lineBuffer), file)) { // fgets returns NULL if EOF or an error occurs
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
      type = DiscType_PS2;
      strncpy(bootPath, valuePtr, CNF_MAX_STR);
      continue;
    }
    if (!strncmp(lineBuffer, "BOOT", 4)) { // PS1 title
      type = DiscType_PS1;
      strncpy(bootPath, valuePtr, CNF_MAX_STR);
      continue;
    }
    if (!strncmp(lineBuffer, "VER", 3)) { // Title version
      strncpy(titleVersion, valuePtr, CNF_MAX_STR);
      continue;
    }
  }
  fclose(file);
  free(cnf);

  // Get the start of the executable path
  valuePtr = strchr(bootPath, '\\');
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
