#include <fcntl.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <ps2sdkapi.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

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
IRX_DEFINE(fileXio);
IRX_DEFINE(ps2dev9);
IRX_DEFINE(ps2atad);
IRX_DEFINE(ps2hdd);
IRX_DEFINE(ps2fs);

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

  IRX_LOAD(iomanX, 0, NULL)
  IRX_LOAD(fileXio, 0, NULL)
  IRX_LOAD(ps2dev9, 0, NULL)
  IRX_LOAD(ps2atad, 0, NULL)
  IRX_LOAD(ps2hdd, 0, NULL)
  IRX_LOAD(ps2fs, 0, NULL)

  fileXioInit();

  // Wait for IOP to initialize device drivers
  for (int attempts = 0; attempts < DELAY_ATTEMPTS; attempts++) {
    ret = open("hdd0:", O_DIRECTORY | O_RDONLY);
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
