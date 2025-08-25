#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>

typedef enum {
  FLAG_SKIP_PS2_LOGO = (1 << 0),  // Skip PS2LOGO when booting discs
  FLAG_DISABLE_GAMEID = (1 << 1), // Disable PixelFX game ID
  FLAG_USE_DKWDRV = (1 << 2),     // Use DKWDRV for PS1 discs
  FLAG_PS1DRV_FAST = (1 << 3),    // If set, will force PS1DRV fast disc speed
  FLAG_PS1DRV_SMOOTH = (1 << 4),  // If set, will force PS1DRV texture smoothing
  FLAG_PS1DRV_USE_VN = (1 << 5),  // If set, run PS1DRV via the PS1DRV Video Mode Negator
} Flags;

typedef enum {
  TRIGGER_TYPE_AUTO = -1,
  TRIGGER_TYPE_START = 1,
  TRIGGER_TYPE_TRIANGLE,
  TRIGGER_TYPE_CIRCLE,
  TRIGGER_TYPE_CROSS,
  TRIGGER_TYPE_SQUARE,
} TriggerType;

// A simple linked list for paths and arguments
typedef struct linkedStr {
  char *str;
  struct linkedStr *next;
} linkedStr;

typedef struct launchPath {
  TriggerType trigger;
  linkedStr *paths;
  linkedStr *args;
  struct launchPath *next;
} launchPath;

// Settings struct, contains option flags and paths
typedef struct {
  uint8_t flags;    // Flags
  launchPath *paths; // Launch paths
} Settings;

// Stores MBR settings
extern Settings settings;

int loadConfig(void);

#endif
