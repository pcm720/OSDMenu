#include "common.h"
#include "patches.h"
#include <iopcontrol.h>
#include <kernel.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>

void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

static void (*mainEPC)(void);

unsigned char selectedVMode;
enum ConsoleRegions consoleRegion = CONSOLE_REGION_INVALID;
enum DiscRegions discRegion = DISC_REGION_INVALID;

static void wipeUserMem(void);
static void invokeSetGsCrt(void);
static int parseROMVER(void);
static void parseDiscRegion(const char *id);

// Expects the following arguments:
// argv[0] — game ID
// argv[1] — version from SYSTEM.CNF
int main(int argc, char *argv[]) {
  if (argc < 2) {
    return -1;
  }

  // Initialize the RPC manager and reboot the IOP
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };
  fioInit();

  // Erase the region where the packed ELF will be loaded to.
  wipeUserMem();

  parseDiscRegion(argv[0]);
  int isPacked = parseROMVER();

  // Determine the video mode to use. If the disc's region can be determined, use the disc's region.
  if (discRegion != DISC_REGION_INVALID) {
    // Select video mode based on the disc's region (Europe = PAL game).
    selectedVMode = (discRegion == DISC_REGION_EUROPE) ? 3 : 2;
  } else {
    // Negate video mode: if region == Europe, set video mode to NTSC. Otherwise, PAL.
    selectedVMode = (consoleRegion == CONSOLE_REGION_EUROPE) ? 2 : 3;
    // Since the disc's region cannot be determined, make an assumption based on the selected video mode.
    discRegion = selectedVMode == 3 ? 'E' : 'J';
  }

  FlushCache(0);

  // Load PS1DRV into memory
  t_ExecData elfData;
  int res = SifLoadElf("rom0:PS1DRV", &elfData);
  fioExit();
  sceSifExitRpc();

  if (res != 0)
    return 0;

  // Patch PS1DRV
  if (isPacked) {
    // For ROMs after v2.00 (v2.20 is the earliest known version), PS1DRV was upgraded to v1.3.0 and is packed.
    // They are also universal, which includes the game compatibility lists for all regions.
    // The region is determined with the ROMVER string.
    hookUnpackFunction((void *)elfData.epc);
  } else {
    // Older ROMs have unpacked PS1DRV programs that are coded for either NTSC or PAL.
    patchPS1DRVVMode();
  }

  mainEPC = (void *)elfData.epc;
  FlushCache(0);
  FlushCache(2);

  argc = 3;
  char *nargv[3] = {"rom0:PS1DRV", argv[0], argv[1]};
  ExecPS2(&invokeSetGsCrt, (void *)elfData.gp, 3, nargv);
}

// Reads ROMVER and parses the console region and ROM version
// Returns 1 if ROM version is >2.00
static int parseROMVER(void) {
  // Determine console region
  char romver[16] = {0};
  int fd = open("rom0:ROMVER", O_RDONLY);
  read(fd, romver, sizeof(romver));
  close(fd);

  switch (romver[4]) {
  case 'C':
    consoleRegion = CONSOLE_REGION_CHINA;
    break;
  case 'E':
    consoleRegion = CONSOLE_REGION_EUROPE;
    break;
  case 'H':
  case 'A':
    consoleRegion = CONSOLE_REGION_USA;
    break;
  case 'J':
    consoleRegion = CONSOLE_REGION_JAPAN;
    break;
  default:
    consoleRegion = CONSOLE_REGION_INVALID;
  }

  if (romver[1] > '1' && romver[2] != '0')
    return 1;

  return 0;
}

static void parseDiscRegion(const char *id) {
  // This may not be the best way to do it, but it seems like the 3rd letter typically indicates the region:
  // SxPx - Japan
  // SxUx - USA
  // SxEx - Europe
  // Some title IDs might be lower-case
  if (strlen(id) >= 3) {
    switch (id[2]) {
    case 'P':
    case 'p':
      discRegion = DISC_REGION_JAPAN;
      break;
    case 'U':
    case 'u':
      discRegion = DISC_REGION_USA;
      break;
    case 'E':
    case 'e':
      discRegion = DISC_REGION_EUROPE;
      break;
    default:
      discRegion = DISC_REGION_INVALID;
    }
  }
}

static void invokeSetGsCrt(void) {
  // This is necessary because PS1DRV runs off the initial state that LoadExecPS2 leaves the console in.
  // Therefore it is necessary to invoke SetGsCrt with the desired video mode,
  // so that the registers that aren't set by PS1DRV will be in the correct state.
  // For example, the PLL mode (54.0MHz for NTSC or 53.9MHz for PAL, for applicable console models that have a PLL setting) is only set within
  // SetGsCrt, but not in PS1DRV.
  SetGsCrt(1, selectedVMode, 1);
  mainEPC();
}

static void wipeUserMem(void) {
  for (int i = 0x100000; i < 0x02000000; i += 64) {
    asm volatile("\tsq $0, 0(%0) \n"
                 "\tsq $0, 16(%0) \n"
                 "\tsq $0, 32(%0) \n"
                 "\tsq $0, 48(%0) \n" ::"r"(i));
  }

  FlushCache(0);
  FlushCache(2);
}
