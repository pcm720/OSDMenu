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

#include <iopcontrol.h>
#include <iopcontrol_special.h>
#include <kernel.h>
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
int loadEmbeddedELF(char *elfPath, char *ioprpPath, ShutdownType dev9ShutdownType, int doIOPReset, int argc, char *argv[]);

// Loads and executes the ELF elfPath points to.
int loadELFFromFile(char *elfPath, char *ioprpPath, ShutdownType dev9ShutdownType, int doIOPReset, int argc, char *argv[]);

// Loads an ELF file from the path specified in argv[0].
// The loader's behavior can be altered by an optional last command-line argument (argv[argc-1]).
// This argument should begin with "-la=", followed by one or more letters that modify the loader's behavior:
//   - 'R': Reset IOP
//   - 'N': Put the HDD into idle mode and keep the rest of DEV9 powered on (HDDUNITPOWER = NIC)
//   - 'D': Keep both the HDD and DEV9 powered on (HDDUNITPOWER = NICHDD)
//   - 'I': The argv[argc-2] argument contains IOPRP image path (for HDD, the path must be a pfs: path on the same partition as the ELF file)
//   - 'E': The argv[argc-2] argument contains ELF memory location to use instead of argv[0]
// Note:
//   - 'D' and 'N' are mutually exclusive; if both are specified, only the last one will take effect.
//   - When using 'I' and 'E' together, the arguments are interpreted in the order they appear:
//     - If specifying "IE", argv[argc-2] is treated as the IOPRP path, and argv[argc-3] as the ELF path.
//     - With "EI", argv[argc-2] is treated as the ELF memory location, and argv[argc-3] as the IOPRP path.
//   - Memory location path for IOPRP image is only supported when the ELF path is also a memory location
//
// The syntax for specifying a memory location is: mem:<8-char address in HEX>:<8-char file size in HEX>
int main(int argc, char *argv[]) {
  // arg[0] is the path to ELF
  if (argc < 1)
    return -EINVAL;

  int doIOPReset = 0;
  ShutdownType dev9ShutdownType = ShutdownType_All;
  char *ioprpPath = NULL;
  char *elfPath = NULL;

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
      default:
      }
    }
    argc--;
  }

  // Init SIF RPC
  sceSifInitRpc(0);

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

  // Handle in-memory ELF file
  if (!strncmp(elfPath, "mem:", 4))
    return loadEmbeddedELF(elfPath, ioprpPath, dev9ShutdownType, doIOPReset, argc, argv);

  return loadELFFromFile(elfPath, ioprpPath, dev9ShutdownType, doIOPReset, argc, argv);
}

// Loads and executes the ELF elfPath points to.
// elfPath must be mem:<8-char address in HEX>:<8-char file size in HEX>
int loadEmbeddedELF(char *elfPath, char *ioprpPath, ShutdownType dev9ShutdownType, int doIOPReset, int argc, char *argv[]) {
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
  return ExecPS2((void *)eh->entry, NULL, argc, argv);
}

// Loads and executes the ELF elfPath points to.
int loadELFFromFile(char *elfPath, char *ioprpPath, ShutdownType dev9ShutdownType, int doIOPReset, int argc, char *argv[]) {
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

  SifLoadFileInit();
  int ret = SifLoadElf(elfPath, &elfdata);
  if (ret && (ret = SifLoadElfEncrypted(elfPath, &elfdata)))
    return ret;
  SifLoadFileExit();

  // Shutdown DEV9
  shutdownDEV9(dev9ShutdownType);

  if (ioprpPath) {
    // Load IOPRP file
    if (loadIOPRP(ioprpPath) < 0)
      return ret;
  } else if (doIOPReset)
    resetIOP();

  sceSifExitRpc();
  if (ret == 0 && elfdata.epc != 0) {
    FlushCache(0);
    FlushCache(2);
    return ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, argc, argv);
  } else {
    return -ENOENT;
  }
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
