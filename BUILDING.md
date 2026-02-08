# Building

OSDMenu uses CMake for dependency management and cross-platform support.

## Basic build

```bash
# Configure CMake and build all targets
cmake -B build
cmake --build build
```
## Building specific targets

You can build specific targets:

```bash
# Build just the launcher
cmake --build build --target launcher

# Build just the OSDMenu MBR
cmake --build build --target osdmbr

# Build OSDMenu or HOSDMenu
cmake --build build --target osdmenu
cmake --build build --target hosdmenu
```

## KELF signing

KELF (XLF) files are automatically built when the following environment variables are set:
- `KELFSERVER_API_ADDRESS` — API server address
- `KELFSERVER_API_KEY` -— API key for signing

## Configuration options

- `RELEASE` — build the release package in `release/` directory (requires KELF variables,  default: `OFF`)
- `DEBUG` — keep debug symbols in uncompressed binaries (default: `OFF`)
- `ENABLE_PRINTF` — enable printf debugging output (applies to both launcher and MBR, default: `OFF`)
- `UDPTTY_IP` — UDPTTY IP address (default: `192.168.1.6`)

Example:
```bash
cmake -B build -DENABLE_PRINTF=ON -DUDPTTY_IP=192.168.1.7 -DDEBUG=ON
```

### Patcher options

- `PATCHER_ENABLE_SPLASH` — enable Free McBoot splash screen (default: `ON`)
- `PATCHER_CNF_FILE` — path to embedded CNF file (relative to source directory, default: `""`)
- `PATCHER_KELF_TYPE` — KELF type (fmcb, etc., default: `fmcb`)

Example:
```bash
cmake -B build -DPATCHER_CNF_FILE=examples/OSDMENU.CNF
```

Note that by disabling the Free McBoot splash screen, you're violating the [Free McBoot License](patcher/LICENSE).

### Launcher options

- `LAUNCHER_OSDM` — support for handling OSDMenu arguments (default: `ON`)
- `LAUNCHER_MMCE` — support for loading ELFs from MMCE (default: `ON`)
- `LAUNCHER_USB` — support for loading ELFs from USB (default: `ON`)
- `LAUNCHER_ATA` — support for loading ELFs from exFAT internal HDD (default: `ON`)
- `LAUNCHER_MX4SIO` — support for loading ELFs from MX4SIO (default: `ON`)
- `LAUNCHER_APA` — support for loading ELFs from APA-formatted HDD (default: `ON`)
- `LAUNCHER_CDROM` — support for loading PS1 and PS2 CDs/DVDs (default: `ON`)
- `LAUNCHER_ILINK` — support for loading ELFs from iLink (default: `OFF`)
- `LAUNCHER_UDPBD` — support for loading ELFs from UDPBD (default: `OFF`)
