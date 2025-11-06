#include "history.h"
#include <ctype.h>
#include <dmaKit.h>
#include <gsKit.h>
#include <kernel.h>
#include <stdint.h>
#include <stdio.h>

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

// Returns 1 if ID is a valid PS2 title ID
int validateTitleID(char *titleID) {
  if ((titleID[4] == '_') && ((titleID[7] == '.') || (titleID[8] == '.'))) {
    return 1;
  }

  return 0;
}

// Updates the history file and shows game ID
void updateLaunchHistory(char *titleID, int showAppID) {
  if (!titleID || titleID[0] == '\0')
    return;

  // Ignore SCPN (PS BBN) prefix
  if (!strncmp(titleID, "SCPN", 4) || !((titleID[4] == '_') && ((titleID[7] == '.') || (titleID[8] == '.')))) {
    if (showAppID)
      // Display game ID even if the title ID is not a valid PS2 title ID or has a BBN-exclusive prefix
      gsDisplayGameID(titleID);
    return;
  }

  updateHistoryFile(titleID);
  gsDisplayGameID(titleID);
  return;
}
