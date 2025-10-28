#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "cnf.h"
#include "loader.h"
#include <stdint.h>
#include <stdio.h>

typedef enum {
  FLAG_SKIP_PS2_LOGO = (1 << 0),  // Skip PS2LOGO when booting discs
  FLAG_DISABLE_GAMEID = (1 << 1), // Disable PixelFX game ID
  FLAG_USE_DKWDRV = (1 << 2),     // Use DKWDRV for PS1 discs
  FLAG_PS1DRV_FAST = (1 << 3),    // If set, will force PS1DRV fast disc speed
  FLAG_PS1DRV_SMOOTH = (1 << 4),  // If set, will force PS1DRV texture smoothing
  FLAG_PS1DRV_USE_VN = (1 << 5),  // If set, run PS1DRV via the PS1DRV Video Mode Negator
  FLAG_PREFER_BBN = (1 << 6),     // If set, will attempt to run PSBBN instead of HOSDSYS on OSD errors
  FLAG_APP_GAMEID = (1 << 7),     // If set, the visual Game ID will be displayed for all applications launched by the OSDMenu MBR
} Flags;

typedef enum {
  TRIGGER_TYPE_AUTO = -1,
  TRIGGER_TYPE_START = 1,
  TRIGGER_TYPE_TRIANGLE,
  TRIGGER_TYPE_CIRCLE,
  TRIGGER_TYPE_CROSS,
  TRIGGER_TYPE_SQUARE,
} TriggerType;

typedef struct launchPath {
  TriggerType trigger;
  linkedStr *paths;
  linkedStr *args;
  int argCount;
  struct launchPath *next;
} launchPath;

// Settings struct, contains option flags and paths
typedef struct {
  uint8_t flags;     // Flags
  launchPath *paths; // Launch paths
  int8_t osdLanguage;
  int8_t osdScreenType;
} Settings;

// Stores MBR settings
extern Settings settings;

int loadConfig(void);

// Attempts to load per-title GSM argument from the GSM_CONF_PATH/HOSDGSM_CONF_PATH depending on the device hint
char *getOSDGSMArgument(char *titleID);

#endif
