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
  char *titleID;
  int skipArgv0;
  int argCount;
  char **args;
  ShutdownType dev9ShutdownType;
} SystemCNFOptions;

// Frees all paths and arguments in SystemCNFOptions.
// Does not free the SystemCNFOptions itself.
void freeSystemCNFOptions(SystemCNFOptions *opts);

// Parses the SYSTEM.CNF file with support for OSDMenu PATINFO extensions
// Returns the executable type or a PIExecType_Error if an error occurs.
ExecType parseSystemCNF(FILE *cnfFile, SystemCNFOptions *opts);

// Parses the SYSTEM.CNF file on CD/DVD into SystemCNFOptions
int parseDiscCNF(SystemCNFOptions *opts);

// Attempts to generate a title ID from path
char *generateTitleID(char *path);

// Adds a new string to linkedStr and returns
linkedStr *addStr(linkedStr *lstr, char *str);

// Frees all elements of linkedStr
void freeLinkedStr(linkedStr *lstr);

#endif
