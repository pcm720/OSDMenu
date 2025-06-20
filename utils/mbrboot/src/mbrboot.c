#include "mbrboot_crypto.h"
#include <debug.h>
#include <string.h>

int handlePSBBNArgs(int argc, char *argv[]) {
  scr_printf("\nDecrypting arguments\n");
  argv = decryptMBRBOOTArgs(&argc, argv);

  for (int i = 0; i < 3; i++)
    scr_printf("argv[%d]: %s\n", i, argv[i]);

  if (!strcmp(argv[1], "BootError"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootClock"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootBrowser"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootOpening"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootWarning"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootIllegal"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootPs1Cd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootPs2Cd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootPs2Dvd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootDvdVideo"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "BootHddApp"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "DnasPs1Emu"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "DnasPs2Native"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "DnasPs2Hdd"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "SkipFsck"))
    scr_printf("Unhandled argument\n");
  else if (!strcmp(argv[1], "Initialize"))
    scr_printf("Unhandled argument\n");

  return -1;
}
