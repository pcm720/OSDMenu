#ifndef _CDROM_H_
#define _CDROM_H_

typedef enum {
  DiscType_PS1,
  DiscType_PS2,
} DiscType;

#define CNF_MAX_STR 256

// Parses SYSTEM.CNF on disc into bootPath, titleID and titleVersion
// Returns disc type or a negative number if an error occurs
int parseDiscCNF(char *bootPath, char *titleID, char *titleVersion);

// Attempts to guess PS1 title ID from volume creation date stored in PVD
const char *getPS1GenericTitleID();

#endif

