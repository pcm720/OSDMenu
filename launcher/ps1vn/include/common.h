#ifndef _COMMON_H_
#define _COMMON_H_

enum ConsoleRegions {
  CONSOLE_REGION_INVALID = -1,
  CONSOLE_REGION_JAPAN = 0,
  CONSOLE_REGION_USA, // USA and HK/SG.
  CONSOLE_REGION_EUROPE,
  CONSOLE_REGION_CHINA,

  CONSOLE_REGION_COUNT
};

enum DiscRegions {
  DISC_REGION_INVALID = -1,
  DISC_REGION_JAPAN = 0,
  DISC_REGION_USA,
  DISC_REGION_EUROPE,

  DISC_REGION_COUNT
};

extern unsigned char selectedVMode;
extern enum ConsoleRegions consoleRegion;
extern enum DiscRegions discRegion;

#endif
