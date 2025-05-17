# HOSDMenu MBR Wrapper

This is a simple wrapper for injecting the HOSDMenu patcher into `hdd0:__mbr`.
Custom `crt0.S` from Neutrino/OPL ee_core ensures `__start` (the program entrypoint) is placed at `0x100008`.

Produces headerless payload suitable for signing with `kelftool` (`payload.bin`).

Supports injecting a custom ELF when `PAYLOAD=<ELF>` is passed to `make`.
Make sure the ELF load address does not interfere with the wrapper code.