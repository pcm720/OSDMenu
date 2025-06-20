# HOSDMenu MBR Wrapper

This is a simple wrapper for injecting the HOSDMenu patcher into `hdd0:__mbr`.

Produces headerless payload suitable for signing with `kelftool` (`payload.bin`).

Supports injecting a custom ELF when `PAYLOAD=<ELF>` is passed to `make`.
Make sure the ELF load address does not interfere with the wrapper code.