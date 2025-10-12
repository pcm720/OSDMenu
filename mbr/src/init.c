#include <fcntl.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <stdio.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <hdd-ioctl.h>

// Number of attempts initModules() will wait for hdd0 before returning
#define DELAY_ATTEMPTS 20

// Macros for loading embedded IOP modules
#define IRX_DEFINE(mod)                                                                                                                              \
  extern unsigned char mod##_irx[] __attribute__((aligned(16)));                                                                                     \
  extern uint32_t size_##mod##_irx

#define IRX_LOAD(mod, argLen, argStr)                                                                                                                \
  ret = SifExecModuleBuffer(mod##_irx, size_##mod##_irx, argLen, argStr, &iopret);                                                                   \
  if (ret < 0)                                                                                                                                       \
    return -1;                                                                                                                                       \
  if (iopret == 1)                                                                                                                                   \
    return -1;

IRX_DEFINE(iomanX);
IRX_DEFINE(secrsif);
IRX_DEFINE(fileXio);
IRX_DEFINE(ps2dev9);
IRX_DEFINE(ps2atad);
IRX_DEFINE(ps2hdd_osd);
IRX_DEFINE(ps2fs);

// ps2hdd module arguments. Up to 4 descriptors, 20 buffers
static char ps2hddArguments[] = "-o"
                                "\0"
                                "4"
                                "\0"
                                "-n"
                                "\0"
                                "20";
// ps2fs module arguments. Up to 2 mounts, 10 descriptors, 40 buffers
char ps2fsArguments[] = "-m"
                        "\0"
                        "2"
                        "\0"
                        "-o"
                        "\0"
                        "10"
                        "\0"
                        "-n"
                        "\0"
                        "40";

// Loads IOP modules
int initModules(void) {
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };

  sceSifInitRpc(0);

  int ret = 0;
  int iopret = 0;
  // Apply patches required to load executables from EE RAM
  if ((ret = sbv_patch_enable_lmb()))
    return ret;
  if ((ret = sbv_patch_disable_prefix_check()))
    return ret;
  if ((ret = sbv_patch_fileio()))
    return ret;

  IRX_LOAD(iomanX, 0, NULL)
  IRX_LOAD(fileXio, 0, NULL)
  IRX_LOAD(secrsif, 0, NULL)
  IRX_LOAD(ps2dev9, 0, NULL)
  IRX_LOAD(ps2atad, 0, NULL)
  IRX_LOAD(ps2hdd_osd, sizeof(ps2hddArguments), ps2hddArguments)
  IRX_LOAD(ps2fs, sizeof(ps2fsArguments), ps2fsArguments)

  if ((ret = SifLoadModule("rom0:SIO2MAN", 0, NULL)) < 0)
    return ret;
  if ((ret = SifLoadModule("rom0:MCMAN", 0, NULL)) < 0)
    return ret;
  if ((ret = SifLoadModule("rom0:MCSERV", 0, NULL)) < 0)
    return ret;
  if ((ret = SifLoadModule("rom0:PADMAN", 0, NULL)) < 0)
    return ret;
  if ((ret = SifLoadModule("rom0:ADDDRV", 0, NULL)) < 0)
    return ret;

  // Wait for IOP to initialize device drivers
  for (int attempts = 0; attempts < DELAY_ATTEMPTS; attempts++) {
    ret = open("hdd0:", O_RDONLY | O_DIRECTORY);
    if (ret >= 0) {
      close(ret);
      return 0;
    }

    ret = 0x01000000;
    while (ret--)
      asm("nop\nnop\nnop\nnop");
  }
  return -1;
}
