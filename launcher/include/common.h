#ifndef _COMMON_H_
#define _COMMON_H_

#include "loader.h"
#include <debug.h>
#include <stdint.h>

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
  Device_APA = (1 << 8),
  Device_CDROM = (1 << 9),
  Device_ROM = (1 << 10),
  //
  Device_Debug = (1 << 11),
} DeviceType;

typedef enum {
  FLAG_BOOT_OSD = (1 << 0),     // Whether the launcher was started from OSDMenu
  FLAG_BOOT_HOSD = (1 << 1),    // Whether the launcher was started from HOSDMenu
  FLAG_BOOT_PATINFO = (1 << 2), // Whether the launcher was started from the hdd partition area
  FLAG_APP_GAMEID = (1 << 3),   // Enables/disables showing the visual Game ID for applications
} LauncherFlags;

// Defines global launcher options
typedef struct {
  uint8_t flags;                 // Whether the launcher was started from OSDMenu
  DeviceType deviceHint;         // The device the launcher has been launched from. Only HDD and memory cards are supported
  int mcHint;                    // Memory card number when the deviceHint is Device_MemoryCard
  char *gsmArgument;             // eGSM argument
  char *titleID;                 // Custom title ID
  ShutdownType dev9ShutdownType; // DEV9 Shutdown type override
  char *ps2LogoRegion;           // PS2LOGO patch region
} launcherOptions;

extern launcherOptions settings;

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
// Initializes APA-formatted HDD and mounts the partition
int initPFS(char *path, DeviceType additionalDevices);

// Mounts the partition specified in path
int mountPFS(char *path);

// Unmounts the partition
void deinitPFS();

// Puts HDD in idle mode and powers off the dev9 device
void shutdownDEV9();

// Parses the launcher argv for global flags.
// Returns the new argc
int parseGlobalFlags(int argc, char *argv[]);

// Loads and executes ELF specified in argv[0]
int LoadELFFromFile(int argc, char *argv[]);

#endif
