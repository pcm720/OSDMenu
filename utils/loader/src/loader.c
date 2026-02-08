/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 Francisco Javier Trujillo Mata <fjtrujy@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Rewritten to support loading ELFs from memory, handle IOPRP images and handle DEV9 shutdown
*/

#include "egsm_api.h"
#include "ps2logo.h"
#include <iopcontrol.h>
#include <iopcontrol_special.h>
#include <kernel.h>
#include <libcdvd-common.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <hdd-ioctl.h>
#include <io_common.h>

void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

#define USER_MEM_START_ADDR 0x100000
#define USER_MEM_END_ADDR 0x2000000

typedef enum { ShutdownType_None, ShutdownType_HDD, ShutdownType_All } ShutdownType;

#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1
typedef struct {
  uint8_t ident[16]; // struct definition for ELF object header
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf_header_t;

typedef struct {
  uint32_t type; // struct definition for ELF program section header
  uint32_t offset;
  void *vaddr;
  uint32_t paddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  uint32_t align;
} elf_pheader_t;

// Global flags used by load functions
// Whether IOP reset should be done before loading the ELF
static int8_t doIOPReset = 0;
// DEV9 shutdown type
static ShutdownType dev9ShutdownType = ShutdownType_All;
// Custom IOPRP path
static char *ioprpPath = NULL;
// Target ELF path
static char *elfPath = NULL;
// eGSM flags
static uint32_t eGSMFlags = 0;
// Whether app argv should start with argv[1]
static uint32_t skipArgv0 = 0;

// Resets IOP
void resetIOP();

// Mounts the partition specified in path
int mountPFS(char *path);

// Puts HDD in idle mode and powers off the dev9 device
void shutdownDEV9(ShutdownType s);

// Attempts to reboot IOP with IOPRP image
int loadIOPRP(char *ioprpPath);

// Loads and executes the ELF elfPath points to.
// elfPath must be mem:<8-char address in HEX>:<8-char file size in HEX>
int loadEmbeddedELF(int argc, char *argv[]);

// Loads and executes the ELF elfPath points to.
int loadELFFromFile(int argc, char *argv[]);

// Parses the loader eGSM argument into eGSM flags
uint32_t parseGSMFlags(char *gsmArg);

// Loads an ELF file from the path specified in argv[0].
// The loader's behavior can be altered by an optional last command-line argument (argv[argc-1]).
// This argument should begin with "-la=", followed by one or more letters that modify the loader's behavior:
//   - 'R': Reset IOP
//   - 'N': Put the HDD into idle mode and keep the rest of DEV9 powered on (HDDUNITPOWER = NIC)
//   - 'D': Keep both the HDD and DEV9 powered on (HDDUNITPOWER = NICHDD)
//   - 'I': The argv[argc-2] argument contains IOPRP image path (for HDD, the path must be a pfs: path on the same partition as the ELF file)
//   - 'E': The argv[argc-2] argument contains ELF memory location to use instead of argv[0]
//   - 'A': Do not pass argv[0] to the target ELF and start with argv[1]
//   - 'G': Force video mode via eGSM. The argv[argc-2] argument contains eGSM arguments:
//          The argument format is inherited from Neutrino GSM and defined as `x:y:z`, where
//          x — Interlaced field mode, when a full height buffer is used by the game for displaying. Force video output to:
//            -      : don't force (default)  (480i/576i)
//            - fp   : force progressive scan (480p/576p)
//          y — Interlaced frame mode, when a half height buffer is used by the game for displaying. Force video output to:
//            -      : don't force (default)  (480i/576i)
//            - fp1  : force progressive scan (240p/288p)
//            - fp2  : force progressive scan (480p/576p line doubling)
//          z — Compatibility mode
//            -      : no compatibility mode (default)
//            - 1    : field flipping type 1 (GSM/OPL)
//            - 2    : field flipping type 2
//            - 3    : field flipping type 3
// Note:
//   - 'D' and 'N' are mutually exclusive; if both are specified, only the last one will take effect.
//   - When using arguments that require argv[argc-2] together, the arguments are interpreted in the order they appear.
//     For example:
//     - If specifying "IE", argv[argc-2] is treated as the IOPRP path, and argv[argc-3] as the ELF path.
//     - With "EI", argv[argc-2] is treated as the ELF memory location, and argv[argc-3] as the IOPRP path.
//
// The syntax for specifying a memory location is: mem:<8-char address in HEX>:<8-char file size in HEX>
int main(int argc, char *argv[]) {
  // arg[0] is the path to ELF
  if (argc < 1)
    return -EINVAL;

  // Init SIF RPC
  sceSifInitRpc(0);

  // Parse loader argument if argv[argc-1] starts with "-la"
  if (!strncmp(argv[argc - 1], "-la=", 4)) {
    char *la = argv[argc - 1];
    int idx = 4;
    while (la[idx] != '\0') {
      switch (la[idx++]) {
      case 'R':
        doIOPReset = 1;
        break;
      case 'N':
        dev9ShutdownType = ShutdownType_HDD;
        break;
      case 'D':
        dev9ShutdownType = ShutdownType_None;
        break;
      case 'I':
        // IOPRP
        ioprpPath = argv[argc - 2];
        argc--;
        break;
      case 'E':
        // ELF loaded into memory
        elfPath = argv[argc - 2];
        argc--;
        break;
      case 'G':
        // Force video mode via eGSM
        eGSMFlags = parseGSMFlags(argv[argc - 2]);
        argc--;
        break;
      case 'A':
        skipArgv0 = 1;
        break;
      default:
      }
    }
    argc--;
  }

  if (!elfPath) {
    if (!strncmp(argv[0], "hdd", 3)) {
      // Mount the partition
      if (mountPFS(argv[0]))
        return -ENODEV;

      // HDD paths usually look as follows: hdd0:<partition name>:pfs:/<path to ELF>
      // However, SifLoadElf needs PFS path, not hdd0:
      // Extract PFS path from the argument
      elfPath = (strstr(argv[0], ":pfs"));
      if (!elfPath)
        elfPath = argv[0];
      else
        elfPath++; // point to 'pfs...'
    } else
      elfPath = argv[0];
  }

  if (skipArgv0) {
    argc--;
    argv = &argv[1];
  }

  // Handle in-memory ELF file
  if (!strncmp(elfPath, "mem:", 4))
    return loadEmbeddedELF(argc, argv);

  return loadELFFromFile(argc, argv);
}

// Loads the ELF sections into memory
// Returns the entrypoint address on success
int loadELF(int elfMem) {
  elf_header_t *eh;
  elf_pheader_t *eph;
  void *pdata;
  int i;

  eh = (elf_header_t *)elfMem;
  if (_lw((uint32_t)&eh->ident) != ELF_MAGIC)
    return -1;

  eph = (elf_pheader_t *)(elfMem + eh->phoff);

  // Scan through the ELF's program headers and copy them into RAM
  for (i = 0; i < eh->phnum; i++) {
    if (eph[i].type != ELF_PT_LOAD)
      continue;

    pdata = (void *)(elfMem + eph[i].offset);
    memcpy(eph[i].vaddr, pdata, eph[i].filesz);
  }

  FlushCache(0);
  FlushCache(2);
  return eh->entry;
}

// Loads and executes the ELF elfPath points to.
// elfPath must be mem:<8-char address in HEX>:<8-char file size in HEX>
int loadEmbeddedELF(int argc, char *argv[]) {
  // Shutdown DEV9
  shutdownDEV9(dev9ShutdownType);

  if (ioprpPath) {
    // Load IOPRP file
    int ret = loadIOPRP(ioprpPath);
    if (ret < 0)
      return ret;
  } else if (doIOPReset)
    resetIOP();

  sceSifExitRpc();
  sceSifExitCmd();

  int elfMem = 0;
  int elfSize = 0;
  // Parse the address
  if (sscanf(elfPath, "mem:%08X:%08X", &elfMem, &elfSize) < 2)
    return -ENOENT;

  // Clear the memory
  if (elfMem >= USER_MEM_START_ADDR) {
    memset((void *)USER_MEM_START_ADDR, 0, elfMem - USER_MEM_START_ADDR);
    memset((void *)(elfMem + elfSize), 0, USER_MEM_END_ADDR - (elfMem + elfSize));
    FlushCache(0);
  }

  int entry = loadELF(elfMem);
  if (entry < 0)
    return -1;

  if (eGSMFlags)
    enableGSM(eGSMFlags);

  return ExecPS2((void *)entry, NULL, argc, argv);
}

// Loads and executes the ELF elfPath points to.
int loadELFFromFile(int argc, char *argv[]) {
  // Handle ELF files
  if (ioprpPath && !strncmp(ioprpPath, "mem:", 4)) {
    // Clear the memory without touching the loaded IOPRP
    int mem = 0;
    int size = 0;
    // Parse the address
    if (sscanf(ioprpPath, "mem:%08X:%08X", &mem, &size) < 2) {
      sceSifExitRpc();
      return -EINVAL;
    }

    if (mem >= USER_MEM_START_ADDR) {
      memset((void *)USER_MEM_START_ADDR, 0, mem - USER_MEM_START_ADDR);
      memset((void *)(mem + size), 0, USER_MEM_END_ADDR - (mem + size));
    }
  } else
    // Clear all user memory
    memset((void *)USER_MEM_START_ADDR, 0, USER_MEM_END_ADDR - USER_MEM_START_ADDR);

  FlushCache(0);

  // Load ELF into memory
  static t_ExecData elfdata;
  elfdata.epc = 0;

  // Init libcdvd if argv[0] points to cdrom or PS2LOGO
  if (!strncmp(argv[0], "cdrom", 5) || !strcmp(argv[0], "rom0:PS2LOGO"))
    sceCdInit(SCECdINIT);

  SifLoadFileInit();
  int ret = SifLoadElf(elfPath, &elfdata);
  if (ret && (ret = SifLoadElfEncrypted(elfPath, &elfdata)))
    return ret;
  SifLoadFileExit();

  FlushCache(0);
  FlushCache(2);

  // Shutdown DEV9
  shutdownDEV9(dev9ShutdownType);

  if (!strcmp(argv[0], "rom0:PS2LOGO")) {
    // Apply PS2LOGO patch, keep libcdvd initialized for PS2LOGO
    patchPS2LOGO(elfdata.epc);
  } else if (!strncmp(argv[0], "cdrom", 5))
    // Deinit libcdvd if argv[0] points to cdrom
    sceCdInit(SCECdEXIT);

  if (ioprpPath) {
    // Load IOPRP file
    if (loadIOPRP(ioprpPath) < 0)
      return ret;
  } else if (doIOPReset)
    resetIOP();

  sceSifExitRpc();
  sceSifExitCmd();

  if (ret != 0 || elfdata.epc == 0)
    return -ENOENT;

  if (eGSMFlags)
    enableGSM(eGSMFlags);

  return ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, argc, argv);
}

// Attempts to reboot IOP with IOPRP image
int loadIOPRP(char *ioprpPath) {
  int res = 0;
  int size = 0;
  if (!strncmp(ioprpPath, "mem:", 4)) {
    // Load IOPRP from memory
    int mem = 0;
    // Parse the address
    if (sscanf(ioprpPath, "mem:%08X:%08X", &mem, &size) < 2) {
      sceSifExitRpc();
      return -EINVAL;
    }

    res = SifIopRebootBuffer((void *)mem, size);
    if (!res && !(res = SifIopRebootBufferEncrypted((void *)mem, size)))
      return -1;

    while (!SifIopSync()) {
    };

    return res;
  }

  // Try to load from path instead
  int fd = fileXioOpen(ioprpPath, FIO_O_RDONLY);
  if (fd < 0)
    return fd;

  // Get the file size
  size = fileXioLseek(fd, 0, FIO_SEEK_END);
  if (size <= 0) {
    fileXioClose(fd);
    return -EIO;
  }
  fileXioLseek(fd, 0, FIO_SEEK_SET);

  // Read IOPRP into memory @ 0x1f00000
  char *mem = (void *)0x1f00000;
  res = fileXioRead(fd, mem, size);
  fileXioClose(fd);
  if (res < size) {
    return -EIO;
  }

  // Reboot IOP with the IOPRP image
  res = SifIopRebootBuffer(mem, size);
  if (!res && !(res = SifIopRebootBufferEncrypted(mem, size)))
    return -1;

  if (res)
    res = 0;
  else
    res = -1;

  while (!SifIopSync()) {
  };

  // Clear the memory
  memset(mem, 0, size);
  return 0;
}

// Mounts the partition specified in path
int mountPFS(char *path) {
  // Extract partition path
  char *filePath = strstr(path, ":pfs:");
  char pathSeparator = '\0';
  if (filePath || (filePath = strchr(path, '/'))) {
    // Terminate the partition path
    pathSeparator = filePath[0];
    filePath[0] = '\0';
  }

  // Mount the partition
  int res = fileXioMount("pfs0:", path, FIO_MT_RDONLY);
  if (pathSeparator != '\0')
    filePath[0] = pathSeparator; // Restore the path
  if (res)
    return -ENODEV;

  return 0;
}

// Puts HDD in idle mode and powers off the dev9 device
void shutdownDEV9(ShutdownType s) {
  // Unmount the partition (if mounted)
  fileXioUmount("pfs0:");
  switch (s) {
  case ShutdownType_HDD:
    // Immediately put HDDs into idle mode
    fileXioDevctl("hdd0:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
    fileXioDevctl("hdd1:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
    break;
  case ShutdownType_All:
    // Immediately put HDDs into idle mode
    fileXioDevctl("hdd0:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
    fileXioDevctl("hdd1:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
    // Turn off dev9
    fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0);
    break;
  default:
  }
}

// Resets IOP
void resetIOP() {
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };
  sceSifInitRpc(0);
}

// Parses the loader eGSM argument into the eGSM flags
uint32_t parseGSMFlags(char *gsmArg) {
  uint32_t flags = 0;
  if (!gsmArg)
    return 0;

  if (!strncmp(gsmArg, "fp", 2)) {
    switch (gsmArg[2]) {
    case '1':
      flags |= EGSM_FLAG_VMODE_FP1;
      break;
    case '2':
      flags |= EGSM_FLAG_VMODE_FP2;
      break;
    default:
      return 0;
    }
    gsmArg += 3;
  } else if (!strncmp(gsmArg, "1080ix", 6)) {
    switch (gsmArg[6]) {
    case '1':
      flags |= EGSM_FLAG_VMODE_1080I_X1;
      break;
    case '2':
      flags |= EGSM_FLAG_VMODE_1080I_X2;
      break;
    case '3':
      flags |= EGSM_FLAG_VMODE_1080I_X3;
      break;
    default:
      return 0;
    }
    gsmArg += 7;
  } else
    return 0;

  if (gsmArg[0] == ':') {
    // Compatibility mode
    gsmArg++;
    switch (gsmArg[0]) {
    case '1': // Mode 1
      flags |= EGSM_FLAG_COMP_1;
      break;
    case '2': // Mode 2
      flags |= EGSM_FLAG_COMP_2;
      break;
    case '3': // Mode 3
      flags |= EGSM_FLAG_COMP_3;
      break;
    }
    gsmArg += 1;
  }

  if (flags) {
    // 576p mode is unsupported on PS2s with ROMVER <210, so check the ROMVER
    // and force disable progressive PAL mode if the console is earlier
    int fd = fileXioOpen("rom0:ROMVER", FIO_O_RDONLY);
    if (fd < 0) // If ROMVER couldn't be opened for some reason, disable 576p just to be safe
      flags |= EGSM_FLAG_NO_576P;
    else {
      char romver[4] = {0};

      // Read ROM version
      fileXioRead(fd, romver, 4);
      fileXioClose(fd);

      if (romver[1] < '2' || romver[2] < '1')
        flags |= EGSM_FLAG_NO_576P;
    }
  }

  return flags;
}
