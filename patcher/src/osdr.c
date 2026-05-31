#include "defaults.h"
#include "settings.h"
#include <iopcontrol.h>
#include <iopcontrol_special.h>
#include <loadfile.h>
#include <osd_config.h>
#include <ps2sdkapi.h>
#include <sifrpc-common.h>
#include <sifrpc.h>
#include <sio.h>
#include <stdint.h>
#include <string.h>
#include <tamtypes.h>
#include <zlib.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>
#include <io_common.h>

#define OSDR_MAGIC 0x5244534f      // "OSDR" little-endian
#define OSDR_MEM 0x1000000         // OSDR load address
#define OSD_OSDSYS_MEM 0x200000    // OSDSYS load address, temporarily used for IOPRP and IRX
#define OSD_RESOURCE_MEM 0x1A00000 // Resource load address

// Resource types
enum OSDRType {
  OSDR_TYPE_IOPRP = 0,
  OSDR_TYPE_IRX = 1,
  OSDR_TYPE_OSDSYS = 2,
  OSDR_TYPE_OSD_RESOURCE = 3,
  OSDR_TYPE_EOF = 0xFFFFFFFF,
};

// OSR resource header
typedef struct {
  uint32_t type;             // Resource type
  uint32_t compressedSize;   // Compressed size
  uint32_t uncompressedSize; // Uncompressed size
  char name[16];             // Resource name
} OSDRResourceHeader;

typedef struct {
  void *m_name;
  void *m_data;
  u32 m_size;
  u32 m_flags;
} OSDSYSResourceInfo;

// DEX OSDSYS patch addresses
void *dexPatchAddresses[] = {
    (void *)0x20A148, // IOP reboot
    (void *)0x20A250, // sceSifSyncIop loop
    (void *)0x20A480, // Resource loading
    (void *)0x209CEC, // XDEV9 presence check
    (void *)0x209C4C, // XDEV9 loading
    (void *)0x209C64, // XDEV9SERV loading
    (void *)0x27D500, // Resource table
};

// CEX OSDSYS patch addresses
void *cexPatchAddresses[] = {
    (void *)0x209FC0, // IOP reboot
    (void *)0x20A0C8, // sceSifSyncIop loop
    (void *)0x20A2F8, // Resource loading
    (void *)0x209B64, // XDEV9 presence check
    (void *)0x209AC4, // XDEV9 loading
    (void *)0x209ADC, // XDEV9SERV loading
    (void *)0x27BC78, // Resource table
};

// Defined in common/defaults.h
char xfromOSDRPath[] = XFROM_OSDR_PATH;
char osdrPath[] = OSDR_PATH;

// Attempts to load OSDSYS resource file into memory
int loadOSDR() {
  // Try XFROM first
  int fd = fioOpen(xfromOSDRPath, FIO_O_RDONLY);
  if (fd < 0) {
    // Check the boot memory card
    if (settings.mcSlot == 1)
      osdrPath[2] = '1';
    else
      osdrPath[2] = '0';

    fd = fioOpen(osdrPath, FIO_O_RDONLY);
  }
  if (fd < 0) {
    // If CNF doesn't exist on boot MC, try the other slot
    if (settings.mcSlot == 1)
      osdrPath[2] = '0';
    else
      osdrPath[2] = '1';
    if ((fd = fioOpen(osdrPath, FIO_O_RDONLY)) < 0)
      return -1;
  }

  int size = fioLseek(fd, 0, FIO_SEEK_END);
  fioLseek(fd, 0, FIO_SEEK_SET);
  fioRead(fd, (void *)OSDR_MEM, size);
  fioClose(fd);
  fioExit();
  return 0;
}

// Unpacks OSDSYS resource file and applies OSDSYS resource patches
int unpackOSDR() {
  if (_lw(OSDR_MEM) != OSDR_MAGIC)
    return -1;

  OSDRResourceHeader *entry = (OSDRResourceHeader *)(OSDR_MEM + 0x4);
  void *dataPtr = NULL;
  void *destPtr = NULL;

  // IRX results
  int isXDEV9Loaded = 0;

  void **patchAddresses = NULL;
  void *resourcePtr = NULL;
  void *resourceDstPtr = (void *)OSD_RESOURCE_MEM;

  char resName[17] = {0};
  int result = 0;
  uint32_t decompSize = 0;
  while (1) {
    dataPtr = (void *)entry + sizeof(OSDRResourceHeader);
    // Set destination address for zlib
    switch (entry->type) {
    case OSDR_TYPE_IOPRP:
    case OSDR_TYPE_IRX:
    case OSDR_TYPE_OSDSYS:
      destPtr = (void *)OSD_OSDSYS_MEM;
      break;
    case OSDR_TYPE_OSD_RESOURCE:
      destPtr = resourceDstPtr;
      break;
    case OSDR_TYPE_EOF:
      // No more resources to load
      goto out;
    default:
      // Unsupported resource
      continue;
    }

    memcpy(resName, entry->name, 16);

    decompSize = entry->uncompressedSize;
    if (entry->uncompressedSize > 0) {
      result = uncompress(destPtr, (long unsigned int *)&decompSize, dataPtr, entry->compressedSize);
      if (result != Z_OK)
        return result;
    }

    switch (entry->type) {
    case OSDR_TYPE_IOPRP:
      // Load OSDSYS IOPRP image
      while (!SifIopRebootBuffer((void *)OSD_OSDSYS_MEM, decompSize))
        ;
      while (!SifIopSync())
        ;
      sceSifInitRpc(0);
      break;
    case OSDR_TYPE_IRX:
      if (SifExecModuleBuffer((void *)OSD_OSDSYS_MEM, decompSize, 0, NULL, &result) > 0) {
        if (!strcmp("XDEV9", resName) || !strcmp("XDEV9SERV", resName))
          isXDEV9Loaded = 1;
      } else if (!strcmp("XDEV9", resName) || !strcmp("XDEV9SERV", resName))
        isXDEV9Loaded = 0;
      break;
    case OSDR_TYPE_OSDSYS:
      // Detect OSD type
      if (_lw((uint32_t)cexPatchAddresses[0]) == 0x0c09c3aa) {
        patchAddresses = cexPatchAddresses;
      } else if (_lw((uint32_t)dexPatchAddresses[0]) == 0x0c09c9ca)
        patchAddresses = dexPatchAddresses;
      else
        return -1;

      resourcePtr = patchAddresses[6];
      break;
    case OSDR_TYPE_OSD_RESOURCE:
      // Patch resource address
      OSDSYSResourceInfo *resinfo = resourcePtr;
      resinfo->m_size = entry->uncompressedSize;
      resinfo->m_data = resourceDstPtr;
      resourcePtr += sizeof(OSDSYSResourceInfo);
      // Ensure 16-byte alignment for resources
      resourceDstPtr = (void *)(((uint32_t)(resourceDstPtr + entry->uncompressedSize) + 15) & ~15);
      break;
    }

    entry = dataPtr + entry->compressedSize;
  }

out:
  sceSifExitRpc();
  // Apply OSDSYS patches
  // NOP out IOP reboot code
  memset(patchAddresses[0], 0, 216);
  // NOP out sceSifSyncIop wait loop
  memset(patchAddresses[1], 0, 12);
  // NOP out code relating to loading resources
  memset(patchAddresses[2], 0, 8);
  // NOP out rom0:XDEV9 presence check
  memset(patchAddresses[3], 0, 8);
  // NOP out rom0:XDEV9 loading code
  memset(patchAddresses[4], 0, 24);
  // NOP out rom0:XDEV9SERV loading code
  memset(patchAddresses[5], 0, 24);
  if (isXDEV9Loaded) {
    // Patch success status
    _sw(0x00001027, (uint32_t)patchAddresses[3]); // nor $v0, $zero, $zero
    _sw(0x00421026, (uint32_t)patchAddresses[4]); // xor $v0, $v0, $v0
    _sw(0x00421026, (uint32_t)patchAddresses[5]); // xor $v0, $v0, $v0
  } else {
    // Patch failure status
    _sw(0x00421026, (uint32_t)patchAddresses[3]); // xor $v0, $v0, $v0
    _sw(0x00001027, (uint32_t)patchAddresses[4]); // nor $v0, $zero, $zero
    _sw(0x00001027, (uint32_t)patchAddresses[5]); // nor $v0, $zero, $zero
  }

  memset((void *)OSDR_MEM, 0, ((uint32_t)dataPtr - OSDR_MEM));
  return 0;
}
