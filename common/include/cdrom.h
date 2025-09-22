#ifndef _CDROM_H_
#define _CDROM_H_

#include <stdio.h>

#define CNF_MAX_STR 256

typedef enum {
  ExecType_Error,
  ExecType_PS1,
  ExecType_PS2,
} ExecType;

// Parses the SYSTEM.CNF file into passed string pointers
// - bootPath (BOOT/BOOT2): boot path
// - titleVersion (VER): title version (optional, argument can be NULL)
// - dev9Power (HDDUNITPOWER): can be NIC or NICHDD (optional)
// - ioprpPath (IOPRP): IOPRP path (optional)
//
// Returns the executable type or a negative number if an error occurs.
// All parameters must have at least CNF_MAX_STR bytes allocated.
ExecType parseSystemCNF(FILE *cnfFile, char *bootPath, char *titleVersion, char *dev9Power, char *ioprpPath);

// Parses the SYSTEM.CNF file on CD/DVD into passed string pointers and attempts to detect the title ID
// - bootPath (BOOT/BOOT2): boot path
// - titleID: detected title ID
// - titleVersion (VER): title version (optional, argument can be NULL)
//
// Returns the executable type or a negative number if an error occurs.
// Except for titleID (max 12 bytes), all other parameters must have CNF_MAX_STR bytes allocated.
int parseDiscCNF(char *bootPath, char *titleID, char *titleVersion);

#endif

