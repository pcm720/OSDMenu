#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "gs.h"
#include <stdint.h>

// Embedded CNF file
#ifdef EMBED_CNF
extern unsigned char embedded_cnf[] __attribute__((aligned(16))) __attribute__((aligned(16)));
extern uint32_t size_embedded_cnf;
// By default, points to embedded_cnf
// Relocated by the patcher to EXTRA_RELOC_ADDR during startup
extern uint8_t *embedded_cnf_addr;
#endif

#define CUSTOM_ITEMS 200 // Max number of items in custom menu
#define NAME_LEN 80      // Max menu item length (incl. the string terminator)

typedef enum {
  FLAG_CUSTOM_MENU = (1 << 0),      // Apply menu patches
  FLAG_SKIP_DISC = (1 << 1),        // Disable disc autolaunch
  FLAG_SKIP_SCE_LOGO = (1 << 2),    // Skip SCE logo on boot
  FLAG_BOOT_BROWSER = (1 << 3),     // Boot directly to MC browser
  FLAG_SCROLL_MENU = (1 << 4),      // Enable infinite scrolling
  FLAG_SKIP_PS2_LOGO = (1 << 5),    // Skip PS2LOGO when booting discs
  FLAG_DISABLE_GAMEID = (1 << 6),   // Disable PixelFX game ID
  FLAG_USE_DKWDRV = (1 << 7),       // Use DKWDRV for PS1 discs
  FLAG_BROWSER_LAUNCHER = (1 << 8), // Apply patches for launching applications from the Browser
  FLAG_PS1DRV_FAST = (1 << 9),      // If set, will force PS1DRV fast disc speed
  FLAG_PS1DRV_SMOOTH = (1 << 10),   // If set, will force PS1DRV texture smoothing
  FLAG_PS1DRV_USE_VN = (1 << 11),   // If set, run PS1DRV via the PS1DRV Video Mode Negator
} PatcherFlags;

// Patcher settings struct, contains all configurable patch settings and menu items
typedef struct {
  uint32_t colorSelected[4];                 // The menu items color when selected
  uint32_t colorUnselected[4];               // The menu items color when not selected
  int menuX;                                 // Menu X coordinate (menu center)
  int menuY;                                 // Menu Y coordinate (menu center), only for scroll menu
  int enterX;                                // "Enter" button X coordinate (at main OSDSYS menu)
  int enterY;                                // "Enter" button Y coordinate (at main OSDSYS menu)
  int versionX;                              // "Version" button X coordinate (at main OSDSYS menu)
  int versionY;                              // "Version" button Y coordinate (at main OSDSYS menu)
  int cursorMaxVelocity;                     // The cursors movement amplitude, only for scroll menu
  int cursorAcceleration;                    // The cursors speed, only for scroll menu
  int displayedItems;                        // The number of menu items displayed, only for scroll menu
  int menuItemIdx[CUSTOM_ITEMS];             // Item index in the config file
  int menuItemCount;                         // Total number of valid menu items
  uint16_t patcherFlags;                     // Patcher options
  char leftCursor[20];                       // The left cursor text, only for scroll menu
  char rightCursor[20];                      // The right cursor text, only for scroll menu
  char menuDelimiterTop[NAME_LEN];           // The top menu delimiter text, only for scroll menu
  char menuDelimiterBottom[NAME_LEN];        // The bottom menu delimiter text, only for scroll menu
  char menuItemName[CUSTOM_ITEMS][NAME_LEN]; // Menu items text
  GSVideoMode videoMode;                     // OSDSYS Video mode (0 for auto)
  char romver[15];                           // ROMVER string, initialized before patching
#ifndef HOSD
  char dkwdrvPath[50]; // Path to DKWDRV
  uint8_t mcSlot;      // Memory card slot contaning currently loaded OSDMENU.CNF
#endif
} PatcherSettings;

// Stores patcher settings and OSDSYS menu items
extern PatcherSettings settings;

int loadConfig(void);
void initConfig(void);

#endif
