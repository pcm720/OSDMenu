
#define NEWLIB_PORT_AWARE
#include "init.h"
#include <debug.h>
#include <fcntl.h>
#include <fileXio_rpc.h>
#include <hdd-ioctl.h>
#include <io_common.h>
#include <kernel.h>
#include <libcdvd-common.h>
#include <libpwroff.h>
#include <ps2sdkapi.h>
#include <stdlib.h>
#include <string.h>

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

int writeMBR(char *buffer, int size);
void enableHDDBOOT();
void copyOSDMBRConfig();

// MBR payload
extern char payload_bin[];
extern int size_payload_bin;
extern char osdmbrcnf_bin[];
extern int size_osdmbrcnf_bin;

int main(int argc, char *argv[]) {
  init_scr();
  scr_setCursor(0);
  scr_clear();
  scr_printf(".\n\n\n\tOSDMenu MBR installer\n\n\n\tInitializing modules...\n\n\n");
  int res = initModules();
  if (res < 0) {
    scr_printf("\tFailed to initialize modules: %d", res);
    goto done;
  }
  res = writeMBR(payload_bin, size_payload_bin);
  if (res < 0) {
    scr_printf("\n\n\tFailed: %d\n", res);
    fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0);
    goto done;
  }

  enableHDDBOOT();

  copyOSDMBRConfig();

  scr_printf("\n\n\tDone\n");
  fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0);

done:
  int ret = 0x05000000;
  while (ret--)
    asm("nop\nnop\nnop\nnop");
  Exit(0);
}

int writeMBR(char *payload, int size) {
  scr_printf("\tInjecting the payload into hdd0:__mbr\n");

  // Buffer for read/write operations
  uint8_t buf[512 + sizeof(hddAtaTransfer_t)] __attribute__((aligned(64)));

  scr_printf("\t\tRetrieving __mbr start sector\n");
  iox_stat_t mbrStat;
  int res = fileXioGetStat("hdd0:__mbr", &mbrStat);
  if (res < 0) {
    scr_printf("\t\tFailed to get __mbr stats: %d\n", res);
    return res;
  }

  uint32_t startSector = mbrStat.private_5 + 0x2000;
  uint32_t remainder = (size & 0x1FF); // Bytes in last sector
  uint32_t sectorCount = (size / 512) + ((remainder) ? 1 : 0);
  uint32_t byteCount = 512;

  scr_printf("\t\tWriting the payload at sector %ld\n", startSector);
  for (int i = 0; i < sectorCount; i++) {
    // If this the last sector and the payload doesn't occupy it fully
    if ((i == (sectorCount - 1)) && (remainder != 0)) {
      // Read the sector into the buffer
      byteCount = remainder;
      ((hddAtaTransfer_t *)buf)->lba = startSector + i;
      ((hddAtaTransfer_t *)buf)->size = 1;
      if ((res = fileXioDevctl("hdd0:", HDIOC_READSECTOR, buf, sizeof(hddAtaTransfer_t), buf + sizeof(hddAtaTransfer_t), 512)) < 0) {
        scr_printf("\n\tFailed to read sector %d/%ld: %d\n", i + 1, sectorCount, res);
        return res;
      }
    }

    // Fill the buffer with payload data
    memcpy(buf + sizeof(hddAtaTransfer_t), payload + 512 * i, byteCount);

    scr_printf("\r\t\tWriting sector %d/%ld", i + 1, sectorCount);
    ((hddAtaTransfer_t *)buf)->lba = startSector + i;
    ((hddAtaTransfer_t *)buf)->size = 1;
    if ((res = fileXioDevctl("hdd0:", HDIOC_WRITESECTOR, buf, 512 + sizeof(hddAtaTransfer_t), NULL, 0)) < 0) {
      scr_printf("\n\t\tFailed to write sector %d/%ld: %d\n", i + 1, sectorCount, res);
      return res;
    }
  }

  // Set MBR start and size
  scr_printf("\n\t\tUpdating OSD MBR data\n");
  if (res >= 0) {
    hddSetOsdMBR_t mbr = {
        .start = startSector,
        .size = sectorCount,
    };
    if ((res = fileXioDevctl("hdd0:", HDIOC_SETOSDMBR, &mbr, sizeof(mbr), NULL, 0)))
      scr_printf("\t\tFailed to set MBR: %d\n", res);
  }

  return res;
}

void enableHDDBOOT() {
  scr_printf("\n\tEnabling MBR boot\n");
  if (!sceCdInit(SCECdINoD)) {
    scr_printf("\t\tFailed to init libcdvd\n");
    return;
  }

  int ret;
  uint32_t res;
  char buf[15];

  do {
    sceCdOpenConfig(0, 0, 1, (u32 *)&res);
  } while (res & 0x9);

  do {
    ret = sceCdReadConfig(buf, (u32 *)&res);
  } while (res & 0x9 || ret == 0);

  do {
    ret = sceCdCloseConfig((u32 *)&res);
  } while (res & 0x9 || ret == 0);

  if ((buf[0] & 0x3) == 0x2) {
    scr_printf("\t\tMBR boot is already enabled\n");
    sceCdInit(SCECdEXIT);
    return;
  }

  // Set the HDDLOAD MBR flag
  buf[0] = (buf[0] & ~0x3) | 0x2;
  do {
    sceCdOpenConfig(0, 1, 1, (u32 *)&res);
  } while (res & 0x9);

  do {
    ret = sceCdWriteConfig(buf, (u32 *)&res);
  } while (res & 0x9 || ret == 0);

  do {
    ret = sceCdCloseConfig((u32 *)&res);
  } while (res & 0x9 || ret == 0);

  sceCdInit(SCECdEXIT);
  scr_printf("\t\tMBR boot was enabled successfully\n");
  return;
}

void copyOSDMBRConfig() {
  // Mount the __sysconf partition
  int res = fileXioMount("pfs0:", "hdd0:__sysconf", FIO_MT_RDWR);
  if (res) {
    scr_printf("\t\tFailed to mount the hdd0:__sysconf partition: %d\n", res);
    return;
  }

  // Check if file exists
  int fd = fileXioOpen("pfs0:/osdmenu/OSDMBR.CNF", FIO_O_RDONLY);
  if (fd >= 0)
    fileXioClose(fd);
  else {
    // If not, create the directory and copy the config file
    fileXioMkdir("pfs0:/osdmenu", 0777);
    scr_printf("\n\tCopying the OSDMenu MBR configuration file\n");
    fd = fileXioOpen("pfs0:/osdmenu/OSDMBR.CNF", FIO_O_RDWR | FIO_O_CREAT);
    if (fd < 0)
      scr_printf("\t\tFailed to create the config file: %d\n", fd);
    else {
      res = fileXioWrite(fd, osdmbrcnf_bin, size_osdmbrcnf_bin);
      if (res < size_osdmbrcnf_bin)
        scr_printf("\t\tFailed to write the config file: %d\n", res);

      fileXioClose(fd);
    }
  }
  fileXioUmount("pfs0:");
}
