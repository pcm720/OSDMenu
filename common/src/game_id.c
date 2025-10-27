#include <ctype.h>
#include <dmaKit.h>
#include <gsKit.h>
#include <kernel.h>
#include <stdint.h>
#include <stdio.h>

// Attempts to generate a title ID from path
void generateTitleIDFromELF(char *path, char *dst) {
  // Try to extract the ELF name
  char *ext = strstr(path, ".ELF");
  if (!ext)
    ext = strstr(path, ".elf");

  // Find the start of the ELF name
  char *elfName = strrchr(path, '/');
  if (!elfName)
    return;

  // Advance to point to the actual name
  elfName++;
  // Temporarily terminate the string at extension,
  // copy the first 11 characters and restore the '.'
  if (ext)
    *ext = '\0';

  strncpy(dst, elfName, 11);

  if (ext)
    *ext = '.';

  // Remove whitespace at the end
  for (int i = 10; i >= 0; i--) {
    if (isspace((int)dst[i])) {
      dst[i] = '\0';
      break;
    }
  }
}

//
// GameID code based on https://github.com/CosmicScale/Retro-GEM-PS2-Disc-Launcher
//

static uint8_t calculateCRC(const uint8_t *data, int len) {
  uint8_t crc = 0x00;
  for (int i = 0; i < len; i++) {
    crc += data[i];
  }
  return 0x100 - crc;
}

// Initializes GS and displays visual game ID
void gsDisplayGameID(const char *gameID) {
  GSGLOBAL *gsGlobal = gsKit_init_global();
  gsGlobal->DoubleBuffering = GS_SETTING_ON;

  dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

  // Initialize the DMAC
  int res;
  if ((res = dmaKit_chan_init(DMA_CHANNEL_GIF)))
    return;

  // Init screen
  gsKit_init_screen(gsGlobal);
  gsKit_display_buffer(gsGlobal); // Switch display buffer to avoid garbage appearing on screen
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);
  gsKit_clear(gsGlobal, GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x00));

  uint8_t data[64] = {0};
  int gidlen = strnlen(gameID, 11); // Ensure the length does not exceed 11 characters

  int dpos = 0;
  data[dpos++] = 0xA5; // detect word
  data[dpos++] = 0x00; // address offset
  dpos++;
  data[dpos++] = gidlen;

  memcpy(&data[dpos], gameID, gidlen);
  dpos += gidlen;

  data[dpos++] = 0x00;
  data[dpos++] = 0xD5; // end word
  data[dpos++] = 0x00; // padding

  int data_len = dpos;
  data[2] = calculateCRC(&data[3], data_len - 3);

  int xstart = (gsGlobal->Width / 2) - (data_len * 8);
  int ystart = gsGlobal->Height - (((gsGlobal->Height / 8) * 2) + 20);
  int height = 2;

  for (int i = 0; i < data_len; i++) {
    for (int j = 7; j >= 0; j--) {
      int x = xstart + (i * 16 + ((7 - j) * 2));
      int x1 = x + 1;
      gsKit_prim_sprite(gsGlobal, x, ystart, x1, ystart + height, 0, GS_SETREG_RGBA(0xFF, 0x00, 0xFF, 0x00));

      uint32_t color = (data[i] >> j) & 1 ? GS_SETREG_RGBA(0x00, 0xFF, 0xFF, 0x00) : GS_SETREG_RGBA(0xFF, 0xFF, 0x00, 0x00);
      gsKit_prim_sprite(gsGlobal, x1, ystart, x1 + 1, ystart + height, 0, color);
    }
  }

  // Execute the queue
  gsKit_queue_exec(gsGlobal);
  gsKit_finish();
  gsKit_sync_flip(gsGlobal);
  gsKit_deinit_global(gsGlobal);
}
