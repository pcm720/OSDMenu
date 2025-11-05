# OSDMenu Launcher

A fully-featured ELF launcher that handles launching ELFs and CD/DVD discs.  
Supports passing arbitrary arguments to an ELF and can also be used standalone as a forwarder.

Supported paths are:
- `mmce?:` — MMCE devices. Can be `mmce0`, `mmce1` or `mmce?`
- `mc?:` — Memory Cards. Can be `mc0`, `mc1` or `mc?`
- `mass:` and `usb:` — USB devices (supported via BDM)
- `ata:` — internal exFAT-formatted HDD (supported via BDM)
- `mx4sio:` — MX4SIO (supported via BDM)
- `ilink:` — i.Link mass storage (supported via BDM, disabled in OSDMenu/HOSDMenu)
- `udpbd:` — UDPBD (supported via BDM, disabled in OSDMenu/HOSDMenu)
- `hdd0:` — internal APA-formatted HDD. Both `:pfs:` and `:PATINFO` paths are supported
- `rom?:` — ROM binaries
- `cdrom` — CD/DVD discs
- `osdm` — special path for OSDMenu patcher

Device support can be enabled and disabled by changing build-time configuration options (see [Makefile](Makefile))  
Supports [OSDMenu-specific SYSTEM.CNF extensions](../mbr/README.md#systemcnf-extensions-for-partition-attribute-area-patinfo-paths).

## Global arguments

The launcher supports the following global arguments:

- `-gsm=<>` — runs the target ELF via the [embedded Neutrino GSM](../utils/egsm/).  
  See [this README](../utils/loader/README.md#egsm) for more information on the argument format.   
  Must always be the last argument. Does not apply to `rom?:` paths.
- `-appid` — enables visual Game ID for applications. The ID is generated from the ELF name (up to 11 characters).
- `-patinfo` — when handling PATINFO paths, will ignore the `BOOT2`/`path` line from SYSTEM.CNF and use the first argument as target ELF path.
- `-titleid=` — when launching ELFs, will use this title ID for the history file and visual game ID (up to 11 characters). Only valid IDs will be written to the history file
- `-dev9=` — will pass DEV9 shutdown flags to the loader. Supported values are:
  - `NICHDD` — will keep both the network adapter and HDD on
  - `NIC` — will keep only the network adapter on 

## Handlers

### `udpbd` handler

Reads PS2 IP address from `mc?:/SYS-CONF/IPCONFIG.DAT`

### `cdrom` handler

Waits for the disc to be detected and launches it.
Supports the following arguments:
- `-nologo` — launches the game executable directly, bypassing `rom0:PS2LOGO`
- `-nogameid` — disables visual game ID
- `-dkwdrv` — when PS1 disc is detected, launches DKWDRV from `mc?:/BOOT/DKWDRV.ELF` instead of `rom0:PS1DRV`
- `-dkwdrv=<path to DKWDRV>` — same as `-dkwdrv`, but with custom DKWDRV path
- `-ps1fast` — will force fast disc speed for PS1 discs when not using DKWDRV
- `-ps1smooth` — will force texture smoothing for PS1 discs when not using DKWDRV
- `-ps1vneg` — will run PS1DRV via the PS1DRV Video Mode Negator

For PS1 CDs with generic executable name (e.g. `PSX.EXE`), attempts to guess the game ID using the volume creation date
stored in the Primary Volume Descriptor, based on the table from [TonyHax International](https://github.com/alex-free/tonyhax/blob/master/loader/gameid-psx-exe.c).

For PS2 CDs/DVDs, the `cdrom` handler will look for the embedded Neutrino GSM setting in
- `mc?:/SYS-CONF/OSDGSM.CNF`
- `hdd0:__sysconf/osdmenu/OSDGSM.CNF` (only when running from HDD)  

See [this](#osdgsmcnf) for more details.

### `osdm` handler
When the launcher receives `osdm:d0:<idx>`, `osdm:d1:<idx>` or `osdm:d9:<idx>`  path as `argv[0]`, it reads `OSDMENU.CNF` from the respective memory card or the hard drive (`osdm:d9`),
searches for `path?_OSDSYS_ITEM_<idx>` and `arg_OSDSYS_ITEM_<idx>` entries and attempts to launch the ELF.

Additionally, the launcher supports parsing the configuration from an arbitrary address when receiving `osdm:a<address>:<CNF file size>:<idx>` as `argv[0]`.

Respects `cdrom_skip_ps2logo`, `cdrom_disable_gameid` and `cdrom_use_dkwdrv` for `cdrom` paths, but only if there are no custom arguments for this entry (`arg_OSDSYS_ITEM`).

### Config handler
When the launcher receives a path that ends with `.CNF`, `.cnf`, `.CFG` or `.cfg`,
it will run the [quickboot handler](#quickboot-handler) using this file.

Config file can be located at any device as long as the device mountpoint is one of the listed above.

### Quickboot handler
When the launcher is started without any arguments, it tries to open `<ELF file name>.CNF`
file at the current working directory
and attempts to launch every path in order.

Quickboot file syntax example:
```ini
boot=boot.elf
path=mmce?:/apps/wle.elf
path=mmce?:/apps/wle2.elf
path=ata:/apps/wle.elf
path=mc?:/BOOT/BOOT.ELF
arg=-testarg
arg=-testarg2
```

`boot` — path relative to the config file  
`path` — absolute paths  
`arg` — global launcher arguments or arguments that will be passed to the ELF file 