
#define NEWLIB_PORT_AWARE
#include "init.h"
#include <debug.h>
#include <fcntl.h>
#include <fileXio_rpc.h>
#include <hdd-ioctl.h>
#include <kernel.h>
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
void poweroff(void *arg);

// MBR payload
extern char payload_bin[];
extern int size_payload_bin;

int main(int argc, char *argv[]) {
  init_scr();
  scr_setCursor(0);
  scr_clear();
  scr_printf(".\nHOSDMenu MBR installer\n\n\n\n\nInitializing modules...\n\n\n");
  int res = initModules();
  if (res < 0) {
    scr_printf("Failed to initialize modules: %d", res);
    goto done;
  }

  scr_printf("Injecting the payload into hdd0:__mbr:\n");
  res = writeMBR(payload_bin, size_payload_bin);
  if (res < 0)
    scr_printf("\n\nFailed: %d\n", res);
  else
    scr_printf("\n\nDone\n");

  fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0);

done:
  int ret = 0x05000000;
  while (ret--)
    asm("nop\nnop\nnop\nnop");
  Exit(0);
}

int writeMBR(char *payload, int size) {
  // Buffer for read/write operations
  uint8_t buf[512 + sizeof(hddAtaTransfer_t)] __attribute__((aligned(64)));

  scr_printf("Retrieving __mbr start sector\n");
  iox_stat_t mbrStat;
  int res = fileXioGetStat("hdd0:__mbr", &mbrStat);
  if (res < 0) {
    scr_printf("Failed to get __mbr stats: %d\n", res);
    return res;
  }

  uint32_t startSector = mbrStat.private_5 + 0x2000;
  uint32_t remainder = (size & 0x1FF); // Bytes in last sector
  uint32_t sectorCount = (size / 512) + ((remainder) ? 1 : 0);
  uint32_t byteCount = 512;

  scr_printf("Writing the payload at sector %ld\n", startSector);
  for (int i = 0; i < sectorCount; i++) {
    // If this the last sector and the payload doesn't occupy it fully
    if ((i == (sectorCount - 1)) && (remainder != 0)) {
      // Read the sector into the buffer
      byteCount = remainder;
      ((hddAtaTransfer_t *)buf)->lba = startSector + i;
      ((hddAtaTransfer_t *)buf)->size = 1;
      if ((res = fileXioDevctl("hdd0:", HDIOC_READSECTOR, buf, sizeof(hddAtaTransfer_t), buf + sizeof(hddAtaTransfer_t), 512)) < 0) {
        scr_printf("\nFailed to read sector %d/%ld: %d\n", i + 1, sectorCount, res);
        return res;
      }
    }

    // Fill the buffer with payload data
    memcpy(buf + sizeof(hddAtaTransfer_t), payload + 512 * i, byteCount);

    scr_printf("\rWriting sector %d/%ld", i + 1, sectorCount);
    ((hddAtaTransfer_t *)buf)->lba = startSector + i;
    ((hddAtaTransfer_t *)buf)->size = 1;
    if ((res = fileXioDevctl("hdd0:", HDIOC_WRITESECTOR, buf, 512 + sizeof(hddAtaTransfer_t), NULL, 0)) < 0) {
      scr_printf("\nFailed to write sector %d/%ld: %d\n", i + 1, sectorCount, res);
      return res;
    }
  }

  // Set MBR start and size
  scr_printf("\nUpdating OSD MBR data\n");
  if (res >= 0) {
    hddSetOsdMBR_t mbr = {
        .start = startSector,
        .size = sectorCount,
    };
    if ((res = fileXioDevctl("hdd0:", HDIOC_SETOSDMBR, &mbr, sizeof(mbr), NULL, 0)))
      scr_printf("Failed to set MBR: %d\n", res);
  }
  return res;
}
