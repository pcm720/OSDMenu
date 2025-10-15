#ifndef _COMMON_H_
#define _COMMON_H_

#include <debug.h>

#define BDM_MOUNTPOINT "mass0:"
#define PFS_MOUNTPOINT "pfs0:"
#define DELAY_ATTEMPTS 20

// Enum for supported devices
typedef enum {
  Device_None = 0,
  Device_Basic = (1 << 0),
  Device_MemoryCard = (1 << 1),
  Device_MMCE = (1 << 2),
  // BDM
  Device_ATA = (1 << 3),
  Device_USB = (1 << 4),
  Device_MX4SIO = (1 << 5),
  Device_iLink = (1 << 6),
  Device_UDPBD = (1 << 7),
  Device_BDM = Device_ATA | Device_USB | Device_MX4SIO | Device_iLink | Device_UDPBD, // For loading common BDM modules
  //
  Device_PFS = (1 << 8),
  Device_CDROM = (1 << 9),
  Device_ROM = (1 << 10),
} DeviceType;

// Defines global launcher options
typedef struct {
  DeviceType deviceHint; // The device the launcher has been launched from. Only HDD and memory cards are supported
  int mcHint;            // Memory card number when the deviceHint is Device_MemoryCard
  char *gsmArgument;     // eGSM argument
} launcherOptions;

extern launcherOptions globalOptions;

// A simple linked list for paths and arguments
typedef struct linkedStr {
  char *str;
  struct linkedStr *next;
} linkedStr;

// Prints a message to the screen and console
void msg(const char *str, ...);

// Prints a message to the screen and console and exits
void fail(const char *str, ...);

// Tests if file exists by opening it
int tryFile(char *filepath);

// Attempts to guess device type from path
DeviceType guessDeviceType(char *path);

// Attempts to convert launcher-specific path into path supported by PS2 modules
char *normalizePath(char *path, DeviceType type);

// Attempts to launch ELF from device and path in argv[0]
int launchPath(int argc, char *argv[]);

// Adds a new string to linkedStr and returns
linkedStr *addStr(linkedStr *lstr, char *str);

// Frees all elements of linkedStr
void freeLinkedStr(linkedStr *lstr);

// Initializes APA-formatted HDD and mounts the partition
int initPFS(char *path, int clearSPU, DeviceType additionalDevices);

// Mounts the partition specified in path
int mountPFS(char *path);

// Unmounts the partition
void deinitPFS();

// Parses the launcher argv for global flags.
// Returns the new argc
int parseGlobalFlags(int argc, char *argv[]);

#endif
