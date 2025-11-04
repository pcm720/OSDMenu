
#include "init.h"
#include "common.h"
#include <ctype.h>
#include <fcntl.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <libcdvd.h>
#include <libpwroff.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Macros for loading embedded IOP modules
#define IRX_DEFINE(mod)                                                                                                                              \
  extern unsigned char mod##_irx[] __attribute__((aligned(16)));                                                                                     \
  extern uint32_t size_##mod##_irx

// Defines moduleList entry for embedded and external modules
#define INT_MODULE(mod, argFunc, deviceType) {#mod, NULL, mod##_irx, &size_##mod##_irx, 0, NULL, deviceType, argFunc}
#define EXT_MODULE(mod, path, argFunc, deviceType) {#mod, path, NULL, NULL, 0, NULL, deviceType, argFunc}

// Embedded IOP modules
IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);

#ifndef USE_ROM_MODULES
IRX_DEFINE(mcman);
IRX_DEFINE(mcserv);
#endif

#ifdef MMCE
#define SIO2MAN
IRX_DEFINE(mmceman);
#endif

#if defined(ATA) || defined(APA)
#define DEV9
#define BDM
IRX_DEFINE(ata_bd);
#endif

#ifdef USB
#define BDM
IRX_DEFINE(usbd_mini);
IRX_DEFINE(usbmass_bd_mini);
#endif

#ifdef MX4SIO
#define SIO2MAN
#define BDM
IRX_DEFINE(mx4sio_bd_mini);
#endif

#ifdef ILINK
#define BDM
IRX_DEFINE(iLinkman);
IRX_DEFINE(IEEE1394_bd_mini);
#endif

#ifdef UDPBD
#define DEV9
#define BDM
IRX_DEFINE(smap_udpbd);
#endif

#ifdef APA
#define DEV9
IRX_DEFINE(ps2hdd_osd);
IRX_DEFINE(ps2fs);
IRX_DEFINE(secrsif);
#endif

#ifdef CDROM
IRX_DEFINE(xparam);
#endif

#ifdef SIO2MAN
IRX_DEFINE(sio2man);
#endif

#ifdef DEV9
IRX_DEFINE(ps2dev9);
#endif

#ifdef BDM
IRX_DEFINE(bdm);
IRX_DEFINE(bdmfs_fatfs);
#endif

#ifdef FMCB
IRX_DEFINE(poweroff);
#endif

#ifdef ENABLE_PRINTF
IRX_DEFINE(smap_udptty);
#endif

// Function used to initialize module arguments.
// Must set argLength and return non-null pointer to a argument string if successful.
// Returned pointer must point to dynamically allocated memory
typedef char *(*moduleArgFunc)(uint32_t *argLength);

typedef struct ModuleListEntry {
  char *name;                     // Module name
  char *path;                     // Module path for external modules
  unsigned char *irx;             // Pointer to IRX module
  uint32_t *size;                 // IRX size. Uses pointer to avoid compilation issues with internal modules
  uint32_t argLength;             // Total length of argument string
  char *argStr;                   // Module arguments
  DeviceType type;                // Target device
  moduleArgFunc argumentFunction; // Function used to initialize module arguments
} ModuleListEntry;

// Argument functions
char *initSMAPArguments(uint32_t *argLength);
char *initPS2HDDArguments(uint32_t *argLength);
char *initPS2FSArguments(uint32_t *argLength);

// List of modules to load
static ModuleListEntry moduleList[] = {
    INT_MODULE(iomanX, NULL, Device_Basic),
    INT_MODULE(fileXio, NULL, Device_Basic),
#ifdef SIO2MAN
    INT_MODULE(sio2man, NULL, Device_Basic),
#else
    EXT_MODULE(sio2man, "rom0:SIO2MAN", NULL, Device_Basic),
#endif
#ifndef USE_ROM_MODULES
    INT_MODULE(mcman, NULL, Device_Basic),
    INT_MODULE(mcserv, NULL, Device_Basic),
#else
    EXT_MODULE(mcman, "rom0:MCMAN", NULL, Device_Basic),
    EXT_MODULE(mcserv, "rom0:MCSERV", NULL, Device_Basic),
#endif
#ifdef MMCE
    INT_MODULE(mmceman, NULL, Device_MMCE),
#endif
#ifdef DEV9
    INT_MODULE(ps2dev9, NULL, Device_ATA | Device_UDPBD | Device_APA | Device_Debug),
#endif
#ifdef BDM
    INT_MODULE(bdm, NULL, Device_BDM | Device_APA),
    INT_MODULE(bdmfs_fatfs, NULL, Device_BDM | Device_APA),
#endif
#if defined(ATA) || defined(APA)
    INT_MODULE(ata_bd, NULL, Device_ATA | Device_APA),
#endif
#ifdef USB
    INT_MODULE(usbd_mini, NULL, Device_USB),
    INT_MODULE(usbmass_bd_mini, NULL, Device_USB),
#endif
#ifdef MX4SIO
    INT_MODULE(mx4sio_bd_mini, NULL, Device_MX4SIO),
#endif
#ifdef ILINK
    INT_MODULE(iLinkman, NULL, Device_iLink),
    INT_MODULE(IEEE1394_bd_mini, NULL, Device_iLink),
#endif
#ifdef ENABLE_PRINTF
    INT_MODULE(smap_udptty, &initSMAPArguments, Device_Debug),
#elif defined(UDPBD)
    INT_MODULE(smap_udpbd, &initSMAPArguments, Device_UDPBD),
#endif
#ifdef APA
    INT_MODULE(ps2hdd_osd, &initPS2HDDArguments, Device_APA),
    INT_MODULE(ps2fs, &initPS2FSArguments, Device_APA),
    INT_MODULE(secrsif, NULL, Device_APA),
#endif
};
#define MODULE_COUNT sizeof(moduleList) / sizeof(ModuleListEntry)

static DeviceType currentDevice = Device_None;

// Initializes IOP modules for given device type
int initModules(DeviceType device, int clearSPU) {
  if ((currentDevice)&device)
    // Do nothing if the drivers are already loaded
    return 0;

  if (currentDevice && !(currentDevice & (Device_APA | Device_ATA)) && !(currentDevice & (Device_APA | Device_ATA)))
    shutdownDEV9();

  int ret = 0;
  int iopret = 0;

  // Initialize the RPC manager and reboot the IOP
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };

  // Clear SPU after OSDSYS
  if (clearSPU)
    SifLoadModule("rom0:CLEARSPU", 0, 0);

  // Initialize the RPC manager
  sceSifInitRpc(0);

  // Apply patches required to load modules from EE RAM
  sbv_patch_enable_lmb();
  sbv_patch_disable_prefix_check();
  sbv_patch_fileio();

  // Load modules
  for (int i = 0; i < MODULE_COUNT; i++) {
    ret = 0;
    iopret = 0;
#ifdef ENABLE_PRINTF
    if (!(device & moduleList[i].type) && !(moduleList[i].type & (Device_Basic | Device_Debug)))
#else
    if (!(device & moduleList[i].type) && (moduleList[i].type != Device_Basic))
#endif
      continue;

    // If module has an arugment function, execute it
    if (moduleList[i].argumentFunction != NULL) {
      moduleList[i].argStr = moduleList[i].argumentFunction(&moduleList[i].argLength);
      if (moduleList[i].argStr == NULL) {
        msg("ERROR: Failed to initialize arguments for module %s\n", moduleList[i].name);
        return -ENOENT;
      }
    }

    if (moduleList[i].path)
      ret = SifLoadModule(moduleList[i].path, moduleList[i].argLength, moduleList[i].argStr);
    else
      ret = SifExecModuleBuffer(moduleList[i].irx, *moduleList[i].size, moduleList[i].argLength, moduleList[i].argStr, &iopret);

#ifdef ENABLE_PRINTF
    if (!strcmp(moduleList[i].name, "smap_udptty"))
      sleep(10);
#endif

    // Delay to prevent ps2hdd module from hanging
    if ((device & Device_APA) && !strcmp(moduleList[i].name, "ata_bd"))
      sleep(1);

    if (ret >= 0)
      ret = 0;
    if (iopret == 1)
      ret = iopret;

    // Clean up arguments
    if (moduleList[i].argStr != NULL)
      free(moduleList[i].argStr);

    if (ret) {
      msg("ERROR: Failed to initialize module %s: %d\n", moduleList[i].name, ret);
      return ret;
    }
  }

  currentDevice = device;
  return 0;
}

// Reboots the IOP and executes a path from ROM via LoadExecPS2
void execROMPath(int argc, char *argv[]) {
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };
  sceSifExitRpc();
  SifLoadFileInit();

  argc--;
  LoadExecPS2(argv[0], argc, &argv[1]);
}

#ifdef FMCB
// Shuts down the console.
// Needs initModules(Device_Basic, 0) to be called first
void shutdownPS2() {
  sceSifInitRpc(0);
  sceCdInit(SCECdINoD);
  SifExecModuleBuffer(poweroff_irx, size_poweroff_irx, 0, NULL, NULL);
  poweroffShutdown();
}
#endif

#ifdef CDROM
// Sets IOP emulation flags for Deckard consoles
// Needs initModules(Device_Basic, 0) to be called first
void applyXPARAM(char *gameID) { SifExecModuleBuffer(xparam_irx, size_xparam_irx, strlen(gameID) + 1, gameID, NULL); }
#endif

// Argument functions

#ifdef UDPBD
// Builds IP address argument for SMAP module
// using mc?:SYS-CONF/IPCONFIG.DAT from memory card
char *initSMAPArguments(uint32_t *argLength) {
  // Try to get IP from IPCONFIG.DAT
  // The 'X' in "mcX" will be replaced with memory card number
  static char ipconfigPath[] = "mcX:/SYS-CONF/IPCONFIG.DAT";

  int ipconfigFd, count;
  char ipAddr[16]; // IP address will not be longer than 15 characters
  for (char i = '0'; i < '2'; i++) {
    ipconfigPath[2] = i;
    // Attempt to open IPCONFIG.DAT
    ipconfigFd = open(ipconfigPath, O_RDONLY);
    if (ipconfigFd >= 0) {
      count = read(ipconfigFd, ipAddr, sizeof(ipAddr) - 1);
      close(ipconfigFd);
      break;
    }
  }

  if ((ipconfigFd < 0) || (count < sizeof(ipAddr) - 1)) {
    msg("ERROR: Failed to read IP address from IPCONFIG.DAT\n");
    return NULL;
  }

  count = 0; // Reuse count as line index
  // In case IP address is shorter than 15 chars
  while (!isspace((unsigned char)ipAddr[count])) {
    // Advance index until we read a whitespace character
    count++;
  }
  ipAddr[count] = '\0';

  char ipArg[19]; // 15 bytes for IP string + 3 bytes for 'ip='
  *argLength = 19;
  char *argStr = calloc(sizeof(char), 19);
  snprintf(argStr, sizeof(ipArg), "ip=%s", ipAddr);
  return argStr;
}
#endif

#ifdef APA
// up to 4 descriptors, 20 buffers
static char ps2hddArguments[] = "-o"
                                "\0"
                                "4"
                                "\0"
                                "-n"
                                "\0"
                                "20";
// Sets arguments for PS2HDD modules
char *initPS2HDDArguments(uint32_t *argLength) {
  *argLength = sizeof(ps2hddArguments);

  char *argStr = malloc(sizeof(ps2hddArguments));
  memcpy(argStr, ps2hddArguments, sizeof(ps2hddArguments));
  return argStr;
}

// up to 10 descriptors, 40 buffers
char ps2fsArguments[] = "-o"
                        "\0"
                        "10"
                        "\0"
                        "-n"
                        "\0"
                        "40";
// Sets arguments for PS2HDD modules
char *initPS2FSArguments(uint32_t *argLength) {
  *argLength = sizeof(ps2fsArguments);

  char *argStr = malloc(sizeof(ps2fsArguments));
  memcpy(argStr, ps2fsArguments, sizeof(ps2fsArguments));
  return argStr;
}
#endif
