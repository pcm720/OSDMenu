#include "splash.h"
#include "gs.h"
#include "splash_bmp.h"

// Initializes GS and displays FMCB splash screen
void gsDisplaySplash(GSVideoMode mode) {
  int splashY = 185;
  if (mode == GS_MODE_PAL) {
    splashY = 247;
  }

  gsInit(mode);
  gsClearScreen();
  gsPrintBitmap((640 - splashWidth) / 2, splashY, splashWidth, splashHeight, splash);

  // Draw OSDMenu visual game ID
  uint8_t data[] = {0xA5, 0x00, 0xA9, 0x07, 'O', 'S', 'D', 'M', 'e', 'n', 'u', 0x00, 0xD5, 0x00};

  int xstart = (gsGetMaxX() / 2) - (sizeof(data) * 8);
  int ystart = gsGetMaxY() - (((gsGetMaxY() / 8) * 2) + 20);

  for (int i = 0; i < sizeof(data); i++) {
    for (int j = 7; j >= 0; j--) {
      int x = xstart + (i * 16 + ((7 - j) * 2));
      int x1 = x + 1;
      gsDrawSprite(x, ystart, x1, ystart + 2, 0, GS_RGBAQ(0xFF, 0x00, 0xFF, 0, 0));

      if ((data[i] >> j) & 1)
        gsDrawSprite(x1, ystart, x1 + 1, ystart + 2, 0, GS_RGBAQ(0x00, 0xFF, 0xFF, 0, 0));
      else
        gsDrawSprite(x1, ystart, x1 + 1, ystart + 2, 0, GS_RGBAQ(0xFF, 0xFF, 0x00, 0, 0));
    }
  }
}
