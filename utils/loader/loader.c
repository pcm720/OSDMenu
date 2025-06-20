/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 Francisco Javier Trujillo Mata <fjtrujy@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Modified to not reset IOP if not explicitly requested
*/

#include <iopcontrol.h>
#include <kernel.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <string.h>

//--------------------------------------------------------------
// Redefinition of init/deinit libc:
//--------------------------------------------------------------
// DON'T REMOVE is for reducing binary size.
// These functions are defined as weak in /libc/src/init.c
//--------------------------------------------------------------
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}

DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

//--------------------------------------------------------------
// Start of function code:
//--------------------------------------------------------------
// Clear user memory
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
static void wipeUserMem(void) {
  for (int i = 0x100000; i < 0x02000000; i += 64) {
    asm volatile("\tsq $0, 0(%0) \n"
                 "\tsq $0, 16(%0) \n"
                 "\tsq $0, 32(%0) \n"
                 "\tsq $0, 48(%0) \n" ::"r"(i));
  }
}

int main(int argc, char *argv[]) {
  static t_ExecData elfdata;
  int ret;

  elfdata.epc = 0;

  // arg[0] is the path to ELF
  if (argc < 1) {
    return -EINVAL;
  }

  int doIOPReset = 0;
  if (!strcmp(argv[0], "-r")) {
    argv++;
    doIOPReset = 1;
  }

  // Initialize
  sceSifInitRpc(0);
  wipeUserMem();

  char *elfPath = NULL;
  if (!strncmp(argv[0], "hdd", 3)) {
    // HDD paths usually look as follows: hdd0:<partition name>:pfs:/<path to ELF>
    // However, SifLoadElf needs PFS path, not hdd0:
    // Extract PFS path from the argument
    elfPath = (strstr(argv[0], ":pfs"));
    if (!elfPath)
      elfPath = argv[0];
    else
      elfPath++; // point to 'pfs...'
  } else
    elfPath = argv[0];

  // Writeback data cache before loading ELF.
  FlushCache(0);

  SifLoadFileInit();
  ret = SifLoadElf(elfPath, &elfdata);
  if (ret) // Try to load KELF if SifLoadElf fails
    ret = SifLoadElfEncrypted(elfPath, &elfdata);
  SifLoadFileExit();

  if (doIOPReset) {
    while (!SifIopReset("", 0)) {
    };
    while (!SifIopSync()) {
    };
  }

  if (ret == 0 && elfdata.epc != 0) {
    FlushCache(0);
    FlushCache(2);
    return ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, argc, argv);
  } else {
    sceSifExitRpc();
    return -ENOENT;
  }
}
