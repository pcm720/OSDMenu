#include <debug.h>
#include <fcntl.h>
#include <kernel.h>
#include <loader.h>
#include <malloc.h>
#include <ps2sdkapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <hdd-ioctl.h>
#include <io_common.h>

#include <iopcontrol.h>
#include <libsecr.h>
#include <sifrpc.h>

const int APA_PT_ATTRIBUTE_OFFSET = 0x1000;

int loadSYSTEMCNF() { return 0; }

int handlePATINFOBoot(int argc, char *argv[]) {
  int fd = fioOpen(argv[2], FIO_O_RDONLY);
  if (fd < 0) {
    scr_printf("Failed to open %s: %d\n", argv[2], fd);
    return fd;
  }

  uint8_t buf[512] = {0};

  int res = fioRead(fd, &buf, 512);
  if (res < 0x40) {
    fioClose(fd);
    scr_printf("Failed to read ELF data: %d\n", res);
    return res;
  }

  uint32_t elfOffset = *((uint32_t *)(&buf[0x30]));
  uint32_t elfSize = *((uint32_t *)(&buf[0x34]));
  scr_printf("ELF offset is %lx\n", elfOffset);
  scr_printf("ELF size is %lx\n", elfSize);

  scr_printf("Seeking to ELF location\n");
  res = fioLseek(fd, elfOffset, FIO_SEEK_SET);
  if (res < 0) {
    fioClose(fd);
    scr_printf("Failed to lseek: %d\n", res);
    return res;
  }

  // Allocate the memory ensuring the buffer is 64-byte aligned and is multiple of 512 bytes
  int bufSize = elfSize;
  if (bufSize % 0x200)
    bufSize = (bufSize + 0x200) & ~0x1FF;
  // uint8_t *elfMem = memalign(64, bufSize * sizeof(uint8_t));
  uint8_t *elfMem = (uint8_t *)0x1000000;
  scr_printf("Reading ELF into memory buffer of size %x @ %p\n", bufSize, elfMem);
  if (!elfMem) {
    fioClose(fd);
    scr_printf("Failed to allocate %x bytes\n", bufSize);
    return -ENOMEM;
  }

  res = fioRead(fd, elfMem, bufSize);
  fioClose(fd);
  if (res < elfSize) {
    scr_printf("Failed to read ELF data: %x\n", res);
    return res;
  }

  if (*(uint32_t *)(&elfMem[0]) != 0x464c457f) {
    scr_printf("Trying to decrypt KELF using SECRMAN\n");
    if (!(res = SecrInit())) {
      scr_printf("Failed to init libsecr: %x\n", res);
      return res;
    }
    elfMem = SecrDiskBootFile(elfMem);
    if (!elfMem) {
      scr_printf("Failed to decrypt\n");
      return -1;
    }
    SecrDeinit();
  }

  scr_printf("Loaded to %p\n", elfMem);
  for (int i = 0; i < 100; i++)
    scr_printf("%02x ", elfMem[i]);

  scr_printf("\n");

  scr_printf("Rebooting IOP\n");
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };
  sceSifInitRpc(0);

  char *nargv[1] = {argv[2]};
  LoadEmbeddedELF(0, elfMem, 1, nargv);
  return 0;
}

int handlePSBBNArgs(int argc, char *argv[]) {
  for (int i = 0; i < argc; i++)
    scr_printf("argv[%d]: %s\n", i, argv[i]);

  if (!strcmp(argv[1], "BootError")) {
    char *nargv[1] = {"BootError"};
    LoadExecPS2("rom0:OSDSYS", 1, nargv);
  } else if (!strcmp(argv[1], "BootClock")) {
    char *nargv[1] = {"BootClock"};
    LoadExecPS2("rom0:OSDSYS", 1, nargv);
  } else if (!strcmp(argv[1], "BootBrowser")) {
    char *nargv[1] = {"BootBrowser"};
    LoadExecPS2("rom0:OSDSYS", 1, nargv);
  } else if (!strcmp(argv[1], "BootOpening")) {
    char *nargv[1] = {"BootOpening"};
    LoadExecPS2("rom0:OSDSYS", 1, nargv);
  } else if (!strcmp(argv[1], "BootWarning")) {
    char *nargv[1] = {"BootWarning"};
    LoadExecPS2("rom0:OSDSYS", 1, nargv);
  } else if (!strcmp(argv[1], "BootIllegal")) {
    char *nargv[1] = {"BootIllegal"};
    LoadExecPS2("rom0:OSDSYS", 1, nargv);
  } else if (!strcmp(argv[1], "BootPs1Cd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootPs2Cd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootPs2Dvd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootDvdVideo"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootHddApp")) {

    char *p = strstr(argv[2], ":PATINFO");
    if (p) {
      *p = '\0';
      return handlePATINFOBoot(argc, argv);
    }

    scr_printf("Unhandled argument\n");
  } else if (!strcmp(argv[1], "DnasPs1Emu"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "DnasPs2Native"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "DnasPs2Hdd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "SkipFsck"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "Initialize")) {
    char *nargv[1] = {"Initialize"};
    LoadExecPS2("rom0:OSDSYS", 1, nargv);
  }

  return -1;
}
