#ifndef _LOADER_H_
#define _LOADER_H_
#include <stdint.h>

typedef enum {
  ShutdownType_All,  // Default
  ShutdownType_None, // NICHDD
  ShutdownType_HDD,  // NIC
} ShutdownType;

typedef struct {
  int argc;    // Argument count for the ELF binary
  char **argv; // ELF arguments; argv[0] must be the path to the binary even if elfMem is set

  void *elfMem; // Optional: Memory location of the ELF binary when loading ELF from memory
  int elfSize;  // Optional: Size of the ELF binary in memory

  char *ioprpPath; // Optional: Path to the IOPRP binary. Will be ignored when ioprpMem is set
  void *ioprpMem;  // Optional: Memory location of the IOPRP binary when loading IOPRP image from memory
  int ioprpSize;   // Optional: Size of the IOPRP binary in memory
  char *eGSM;      // Optional: eGSM argument

  int skipArgv0;
  int resetIOP;                  // Indicates if IOP reset is needed (1 = yes). If used with IOPRP, it will be ignored
  ShutdownType dev9ShutdownType; // Type of shutdown to perform for DEV9
} LoadOptions;

// Loads ELF using specified options
int loadELF(LoadOptions *options);

#endif
