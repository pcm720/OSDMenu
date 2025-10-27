#ifndef _CNF_H_
#define _CNF_H_

#include "loader.h"
#include <stdio.h>

#define CNF_MAX_STR 256

typedef enum {
  ExecType_Error,
  ExecType_PS1,
  ExecType_PS2,
} ExecType;

// A simple linked list for paths and arguments
typedef struct linkedStr {
  char *str;
  struct linkedStr *next;
} linkedStr;

// SYSTEM.CNF contents
typedef struct {
  char *bootPath;
  char *ioprpPath;
  char *titleVersion;
  int skipArgv0;
  int argCount;
  char **args;
  ShutdownType dev9ShutdownType;
} SystemCNFOptions;

// Parses the SYSTEM.CNF file with support for OSDMenu PATINFO extensions
// Returns the executable type or a PIExecType_Error if an error occurs.
ExecType parseSystemCNF(FILE *cnfFile, SystemCNFOptions *opts);

// Parses the SYSTEM.CNF file on CD/DVD into passed string pointers and attempts to detect the title ID
// - bootPath (BOOT/BOOT2): boot path
// - titleID: detected title ID
// - titleVersion (VER): title version (optional, argument can be NULL)
//
// Returns the executable type or a negative number if an error occurs.
// Except for titleID (max 12 bytes), all other parameters must have CNF_MAX_STR bytes allocated.
int parseDiscCNF(char *bootPath, char *titleID, char *titleVersion);

// Adds a new string to linkedStr and returns
linkedStr *addStr(linkedStr *lstr, char *str);

// Frees all elements of linkedStr
void freeLinkedStr(linkedStr *lstr);

#endif
