#include "patches_common.h"
#include "patterns_version.h"
#include "settings.h"
#include <debug.h>
#include <stdint.h>
#include <string.h>

#ifdef HOSD
#define OSDMENU_TITLE "HOSDMenu"
#else
#define OSDMENU_TITLE "OSDMenu"
#endif

//
// Version info menu patch
// Injects custon strings into Version submenu
//

// Returns a pointer to sceGsGParam
// Can't use PS2SDK libgs function because these parameters
// are set by OSDSYS at an unknown address when setting the video mode
sceGsGParam *(*sceGsGetGParam)(void) = NULL;

// Static variables
static char romverValue[] = "\ar0.80VVVVRTYYYYMMDD\ar0.00";
static char mechaconRev[] = "0.00 (Debug)";
static char eeRevision[5] = {0};
static char gsRevision[5] = {0};

static uint16_t *(*sceCdApplySCmd)(uint16_t cmdNum, const void *inBuff, uint16_t inBuffSize, void *outBuff) = NULL;

// Initializes version info menu strings
static void (*versionInfoInit)(void);
uint32_t verinfoStringTableAddr = 0;

typedef struct {
  char *name;
  char *value;               // Used for static values
  char *(*valueFunc)();      // Used for dynamic values
  char *(*valueFuncProto)(); // Used for dynamic values on Protokernels
} customVersionEntry;

char *getVideoMode();
char *getGSRevision();
char *getMechaConRevision();
char *getPatchVersionProto() { return GIT_VERSION; }
char *getPatchVersion() { return "\ar0.80" GIT_VERSION "\ar0.00"; }

// Table for custom menu entries
// Supports dynamic variables that will be updated every time the version menu opens
customVersionEntry entries[] = {
    {"Video Mode", NULL, getVideoMode, getVideoMode},             //
    {OSDMENU_TITLE, NULL, getPatchVersion, getPatchVersionProto}, //
    {"ROM", romverValue, NULL},                                   //
    {"Emotion Engine", eeRevision, NULL},                         //
    {"Graphics Synthesizer", NULL, getGSRevision, getGSRevision}, //
    {"MechaCon", NULL, getMechaConRevision, getMechaConRevision}, //
};

// This function will be called every time the version menu opens
void versionInfoInitHandler() {
#ifndef HOSD
  // Execute the original init function
  versionInfoInit();
#endif

  // Extend the string table used by the version menu drawing function.
  // It picks up the entries automatically and stops once it gets a NULL pointer (0)
  //
  // Each table entry is represented by three words:
  // Word 0 — pointer to entry name string
  // Word 1 — pointer to entry value string
  // Word 2 — indicates whether the entry has a submenu and points to:
  // 1. ROMs <2.00 — a list of newline-separated submenu entries where each menu entry is represented
  //  as a comma-separated list of strings (e.g. 'Disc Speed,Standard,Fast\nTexture Mapping,Standard,Smooth\n').
  //  Used to build the menu, but not used to draw it.
  // 2. ROMs >=2.00 — some unknown value that doesn't seem to be used by the menu functions as modifying it doesn't seem to change anything
  // 3. HDD-OSD — points to some sort of submenu index, 00 is Diagnosis submenu

  // Find the first empty entry or an entry with the name located in the patcher address space (<0x100000)
  uint32_t ptr = verinfoStringTableAddr;
  while (1) {
    if ((_lw(ptr) < 0x100000) || (!_lw(ptr) && !_lw(ptr + 4) && !_lw(ptr + 8)))
      break;

    ptr += 12;
  }
  if (ptr == verinfoStringTableAddr) {
    return;
  }

  // Add custom entries
  char *value = NULL;
  for (int i = 0; i < sizeof(entries) / sizeof(customVersionEntry); i++) {
    value = NULL;
    if (entries[i].valueFunc)
      value = entries[i].valueFunc();
    else if (entries[i].value)
      value = entries[i].value;

    if (!value)
      continue;

    _sw((uint32_t)entries[i].name, ptr);
    _sw((uint32_t)value, ptr + 4);
#ifndef HOSD
    _sw(0x00, ptr + 8);
#else
    _sw(0xff, ptr + 8); // Set to fixed index to avoid entries showing up as "Diagnosis" in HDD-OSD
#endif
    ptr += 12;
  }

#ifdef HOSD
  // Execute the original init function
  versionInfoInit();
#endif
};

// Formats single-byte number into M.mm string.
// dst is expected to be at least 5 bytes long
void formatRevision(char *dst, uint8_t rev) {
  dst[0] = '0' + (rev >> 4);
  dst[1] = '.';
  dst[2] = '0' + ((rev & 0x0f) / 10);
  dst[3] = '0' + ((rev & 0x0f) % 10);
  dst[4] = '\0';
}

// Extends version menu with custom entries by overriding the function called every time the version menu opens
void patchVersionInfo(uint8_t *osd) {
  // Find the function that inits version menu entries
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternVersionInit, (uint8_t *)patternVersionInit_mask, sizeof(patternVersionInit));
  if (!ptr)
    return;

#ifndef HOSD
  // First pattern word is nop, advance ptr to point to the function call
  ptr += 4;
#endif

  // Get the original function call and save the address
  uint32_t tmp = _lw((uint32_t)ptr);
  tmp &= 0x03ffffff;
  tmp <<= 2;
  versionInfoInit = (void *)tmp;

#ifndef HOSD
  // Find the string table address in versionInfoInit
  // Even if it's the same in all ROM versions >=1.20, this acts as a basic sanity check
  // to make sure the patch is replacing the actual versionInfoInit
  uint8_t *tableptr = findPatternWithMask((uint8_t *)versionInfoInit, 0x200, (uint8_t *)patternVersionStringTable,
                                          (uint8_t *)patternVersionStringTable_mask, sizeof(patternVersionStringTable));
  if (!tableptr)
    return;

  // Assemble the table address
  tmp = (_lw((uint32_t)tableptr) & 0xFFFF) << 16;
  tmp |= (_lw((uint32_t)tableptr + 8) & 0xFFFF);

  if (tmp > 0x100000 && tmp < 0x2000000)
    // Make sure the address is in the valid address space
    verinfoStringTableAddr = tmp;
  else
    return;
#else
  // Using fixed offset for HDD-OSD to avoid unneeded tracing
  verinfoStringTableAddr = hddosdVerinfoStringTableAddr;
#endif

  // Replace versionInfoInit with the custom function
  tmp = 0x0c000000;
  tmp |= ((uint32_t)versionInfoInitHandler >> 2);
  _sw(tmp, (uint32_t)ptr); // jal versionInfoInitHandler

  // Find sceGsGetGParam address
  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternGsGetGParam, (uint8_t *)patternGsGetGParam_mask, sizeof(patternGsGetGParam));
  if (ptr) {
    tmp = _lw((uint32_t)ptr);
    tmp &= 0x03ffffff;
    tmp <<= 2;
    sceGsGetGParam = (void *)tmp;
  }

  // Find sceCdApplySCmd address
  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternCdApplySCmd, (uint8_t *)patternCdApplySCmd_mask, sizeof(patternCdApplySCmd));
  if (ptr) {
    uint32_t fnptr = (uint32_t)ptr;
    while ((_lw(fnptr) & 0xffff0000) != 0x27bd0000)
      fnptr -= 4;

    sceCdApplySCmd = (void *)fnptr;
  }

  // Initialize static values
  // ROM version
  if (settings.romver[0] != '\0') {
    memcpy(&romverValue[6], settings.romver, 14);
  } else {
    romverValue[0] = '-';  // Put placeholer value
    romverValue[1] = '\0'; // Put placeholer value
  }

  // EE Revision
  formatRevision(eeRevision, GetCop0(15));
}

char *getVideoMode() {
  uint16_t vmode;
  if (sceGsGetGParam) {
    sceGsGParam *gParam = sceGsGetGParam();
    vmode = gParam->sceGsOutMode;
  } else {
    // This function doesn't exist on protokernel,
    // so try using video mode from settings
    vmode = settings.videoMode;
    if (!vmode)
      vmode = GS_MODE_NTSC; // Default to NTSC
  }

  switch (vmode) {
  case GS_MODE_PAL:
    return "PAL";
  case GS_MODE_NTSC:
    return "NTSC";
  case GS_MODE_DTV_480P:
    return "480p";
  case GS_MODE_DTV_1080I:
    return "1080i";
  }

  return "-";
}

char *getGSRevision() {
  uint8_t rev = (*GS_REG_CSR >> 16) & 0xFF;
  if (rev) {
    formatRevision(gsRevision, rev);
    return gsRevision;
  }

  gsRevision[0] = '-';
  gsRevision[1] = '\0';
  return gsRevision;
}

char *getMechaConRevision() {
  if (!sceCdApplySCmd || mechaconRev[0] == '\0')
    return NULL;

  if (mechaconRev[0] != '0')
    // Retrieve the revision only once
    return mechaconRev;

  // sceCdApplySCmd response is always 16 bytes and will corrupt memory with a smaller buffer
  // byte 0 - status byte, byte 1 - major, byte 2 - minor
  uint8_t outBuffer[16] = {0};

  if (sceCdApplySCmd(0x03, outBuffer, 1, outBuffer)) {
    if (outBuffer[1] > 4) {
      // If major version is >=5, clear the last bit (DTL flag on Dragon consoles)
      if (!(outBuffer[2] & 0x1))
        mechaconRev[4] = '\0';

      outBuffer[2] &= 0xFE;
    } else
      mechaconRev[4] = '\0';

    mechaconRev[0] = outBuffer[1] + '0';
    mechaconRev[2] = '0' + (outBuffer[2] / 10);
    mechaconRev[3] = '0' + (outBuffer[2] % 10);
    return mechaconRev;
  }

  // Failed to get the revision
  mechaconRev[0] = '\0';
  return NULL;
}

#ifndef HOSD
//
// Protokernel patches
//

// getDVDPlayerVersion attempts to get DVD player version and writes
// "DVD Player" to label, version to value and terminates the submenu
// or terminates all three if it couldn't get the version. Returns player version.
static char *(*getDVDPlayerVersion)(char *label, char *value, char *submenu) = NULL;

// This function will be called every time the version menu opens
char *versionInfoInitHandlerProtokernel(char *label, char *value, char *submenu) {
  // Execute the original function
  char *res = getDVDPlayerVersion(label, value, submenu);

  // Extend the string table used by the version menu drawing function.
  // It picks up the entries automatically and stops once it reads an empty string
  //
  // Each table entry is represented as follows:
  // First 32 bytes — entry name string
  // 16 bytes — entry value string
  // 1024 bytes — submenu as a list of newline-separated submenu entries where each menu entry is represented
  //  as a comma-separated list of strings (e.g. 'Disc Speed,Standard,Fast\nTexture Mapping,Standard,Smooth\n').
  //  Used to build the menu, but not used to draw it.

  // Find the first empty entry
  uint32_t ptr = (uint32_t)label;
  while (((char *)ptr)[0] != '\0')
    // Go to the next offset
    ptr += 0x430;

  // Add custom entries
  char *cValue = NULL;
  for (int i = 0; i < sizeof(entries) / sizeof(customVersionEntry); i++) {
    cValue = NULL;
    if (entries[i].valueFunc)
      cValue = entries[i].valueFuncProto();
    else if (entries[i].value)
      cValue = entries[i].value;

    if (!cValue)
      continue;

    strncpy((char *)ptr, entries[i].name, 31);
    strncpy((char *)ptr + 0x20, cValue, 15);
    _sw(0, ptr + 0x30);

    ptr += 0x430;
  }

  return res;
}

// Protokernels use a much simpler version of the init function that just loads
// static values into the pre-defined locations (except for DVD Player version)
// Thankfully, the drawing function is dynamic.
// Extends version menu with custom entries by overriding the function called every time the version menu opens
void patchVersionInfoProtokernel(uint8_t *osd) {
  // Find the function that inits version menu entries
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternVersionInit_Proto, (uint8_t *)patternVersionInit_Proto_mask,
                                     sizeof(patternVersionInit_Proto));
  if (!ptr)
    return;

  // Advance ptr to point to the function call
  ptr += 8;

  // Get the original function call and save the address
  uint32_t tmp = _lw((uint32_t)ptr);
  tmp &= 0x03ffffff;
  tmp <<= 2;
  getDVDPlayerVersion = (void *)tmp;

  // Replace versionInfoInit with the custom function
  tmp = 0x0c000000;
  tmp |= ((uint32_t)versionInfoInitHandlerProtokernel >> 2);
  _sw(tmp, (uint32_t)ptr); // jal versionInfoInitHandlerProtokernel

  // Find sceCdApplySCmd address
  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternCdApplySCmd_Proto, (uint8_t *)patternCdApplySCmd_Proto_mask,
                            sizeof(patternCdApplySCmd_Proto));
  if (ptr) {
    uint32_t fnptr = (uint32_t)ptr;
    while ((_lw(fnptr) & 0xffff0000) != 0x27bd0000)
      fnptr -= 4;

    sceCdApplySCmd = (void *)fnptr;
  }

  // Initialize static values
  // ROM version. Protokernels don't support control characters so it might not fit.
  if (settings.romver[0] != '\0') {
    strncpy(romverValue, settings.romver, 15);
  } else {
    romverValue[0] = '-';  // Put placeholer value
    romverValue[1] = '\0'; // Put placeholer value
  }

  // EE Revision
  formatRevision(eeRevision, GetCop0(15));
}
#endif
