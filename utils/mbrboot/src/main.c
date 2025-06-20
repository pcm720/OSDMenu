#include "mbrboot.h"
#include <kernel.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

#include <debug.h>

int main(int argc, char *argv[]) {
  if (!strcmp(argv[0], "rom0:MBRBOOT")) {
    init_scr();
    scr_setCursor(0);
    scr_clear();
    scr_printf("\n\n\n\nargc: %d\n", argc);
    for (int i = 0; i < argc; i++)
      scr_printf("argv[%d]: %s\n", i, argv[i]);

    handlePSBBNArgs(argc, argv);

    __builtin_trap();

    int ret = 0x20000000;
    while (ret--)
      asm("nop\nnop\nnop\nnop");
  }
}
