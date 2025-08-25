#ifndef _CDROM_H_
#define _CDROM_H_

typedef enum {
  DiscType_PS1,
  DiscType_PS2,
} DiscType;

#define CNF_MAX_STR 256

// Parses the SYSTEM.CNF file into passed string pointers
// - bootPath (BOOT/BOOT2): boot path
// - titleID: detected title ID
// - titleVersion (VER): title version (optional, argument can be NULL)
// - dev9Power (HDDUNITPOWER): can be NIC or NICHDD (optional)
// - ioprpPath (IOPRP): IOPRP path (optional)
//
// Returns disc type or a negative number if an error occurs.
// Except for titleID (max 12 bytes), all other arguments must have at least CNF_MAX_STR bytes allocated.
int parseSystemCNF(char *bootPath, char *titleID, char *titleVersion, char *dev9Power, char *ioprpPath);

// Attempts to guess PS1 title ID from volume creation date stored in PVD
const char *getPS1GenericTitleID();

#endif

