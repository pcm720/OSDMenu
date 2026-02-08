#include <kernel.h>
#include <stdint.h>

uint8_t isPAL = 0;

int getConsoleRegion() {
  if (isPAL)
    return 2;
  return 0;
}

// Does the actual patching
int doPatchWithOffsets(uint32_t getRegionLoc, uint32_t checksumLoc) {
  if ((_lw(getRegionLoc) != 0x27bdfff0) || ((_lw(getRegionLoc + 8) & 0xff000000) != 0x0c000000))
    return -1;
  if ((_lw(checksumLoc) & 0xffff0000) != 0x8f820000)
    return -1;

  _sw((0x0c000000 | ((uint32_t)getConsoleRegion >> 2)),
      getRegionLoc + 8);        // Patch function call at getRegionLoc + 8 to always return the disc's region
  _sw(0x24020000, checksumLoc); // Replace load from memory with just writing zero to v0 to bypass logo checksum check

  FlushCache(0);
  FlushCache(2);
  return 0;
}

// Patches PS2LOGO region checks
void doPatch() {
  // ROM 1.10
  // 0x100178 — region getter function, with ROMVER check call at +8 bytes. Required to display the logo properly
  // 0x100278 — logo checksum check
  if (!doPatchWithOffsets(0x100178, 0x100278))
    return;
  // ROM 1.20-1.70
  // 0x102078 — region getter function, with ROMVER check call at +8 bytes. Required to display the logo properly
  // 0x102178 — logo checksum check
  if (!doPatchWithOffsets(0x102078, 0x102178))
    return;
  // ROM 1.80-2.10
  // 0x102018 — region getter function, with ROMVER check call at +8 bytes. Required to display the logo properly
  // 0x102118 — logo checksum check
  if (!doPatchWithOffsets(0x102018, 0x102118))
    return;
  // ROM 2.20+
  // 0x102018 — region getter function, with ROMVER check call at +8 bytes. Required to display the logo properly
  // 0x102264 — logo checksum check
  if (!doPatchWithOffsets(0x102018, 0x102264))
    asm volatile("j 0x100000"); // Jump to PS2LOGO entrypoint
}

// Wraps ExecPS2 call for ROM 2.00
void patchedExecPS2(void *entry, void *gp, int argc, char *argv[]) {
  doPatch();
  ExecPS2(entry, gp, argc, argv);
}

void patchPS2LOGO(uint32_t epc, int isPS2LOGOPAL) {
  isPAL = isPS2LOGOPAL;

  if (epc > 0x1000000) { // Packed PS2LOGO
    if ((_lw(0x1000200) & 0xff000000) == 0x08000000) {
      // ROM 2.20+
      // Replace the jump to 0x100000 in the unpacker with the patching function
      _sw((0x08000000 | ((uint32_t)doPatch >> 2)), (uint32_t)0x1000200);
    } else if ((_lw(0x100011c) & 0xff000000) == 0x0c000000)
      // ROM 2.00
      // Hijack the unpacker's ExecPS2 call to execute the patching function
      _sw((0x0c000000 | ((uint32_t)patchedExecPS2 >> 2)), (uint32_t)0x100011c);
    return;
  }
  if (epc > 0x200000)
    return; // Ignore protokernels

  // Patch unpacked PS2LOGO
  doPatch();
}
