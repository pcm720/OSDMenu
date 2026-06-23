# OSDMenu MBR

A custom `__mbr` payload designed for HOSDMenu, HDD-OSD and PSBBN.  
Supports running arbitrary paths from the HDD, memory cards and XFROM (only on PSX).  

## Installation

### HDD

1. Edit the [OSDMBR.CNF](../examples/OSDMBR.CNF) as you see fit and copy it to `hdd0:__sysconf:pfs:osdmenu/OSDMBR.CNF`
2. Install the payload into the `__mbr` of your HDD using the [OSDMenu MBR Installer](../utils/installer/README.md) or any other way.

### XFROM (PSX)

1. Edit the [OSDMBR.CNF](../examples/OSDMBR.CNF) as you see fit and copy it to `xfrom:/osdmenu/OSDMBR.CNF`
2. Replace the original `xfrom:/BIEXEC-SYSTEM/xosdmain.elf` with `XOSDMBR.XLF`

## Configuration

The OSDMenu MBR can be configured with the `OSDMBR.CNF` file located at `hdd0:__sysconf:pfs:osdmenu/OSDMBR.CNF` or `xfrom:/osdmenu/OSDMBR.CNF`.  
See the [OSDMBR.CNF](../examples/OSDMBR.CNF) example configuration file for hints on how to configure the MBR.

### Configuration options

Multiple paths and arguments are supported for every `boot_` entry.

1. `boot_<key>` — boot paths for the given gamepad key
2. `boot_<key>_arg` — arguments for the given gamepad key  
  Supported keys:
    - `auto` — used when no key is pressed
    - `start`
    - `select`
    - `triangle`
    - `circle`
    - `cross`
    - `square`
    - `up`
    - `down`
    - `left`
    - `right`
    - `l1`
    - `l2`
    - `r1`
    - `r2`
3. `cdrom_skip_ps2logo` — enables or disables running discs via `rom0:PS2LOGO`
4. `cdrom_disable_gameid` — disables or enables visual Game ID
5. `cdrom_use_dkwdrv` — enables or disables launching DKWDRV for PS1 discs  
   OSDMenu MBR will look for DKWDRV at `hdd0:__system:pfs:/osdmenu/DKWDRV.ELF` and `xfrom:/osdmenu/DKWDRV.ELF`
6. `ps1drv_enable_fast` — will enable fast disc speed for PS1 discs when not using DKWDRV
7. `ps1drv_enable_smooth` — will enable texture smoothing for PS1 discs when not using DKWDRV
8. `ps1drv_use_ps1vn` — will run PS1DRV using the PS1DRV Video Mode Negator
9. `prefer_bbn` — makes the MBR to fallback to PSBBN instead of HOSDMenu/HDD-OSD on errors
10. `app_gameid` — if enabled, the visual Game ID will be displayed for all applications launched by the OSDMenu MBR
11. `osd_screentype` — if set, will set the screen type by changing the flag in MechaCon NVRAM. Valid values are `4:3`, `16:9`, `full`
12. `osd_language` — if set, will set the OSDSYS/HDD-OSD launguage by changing the language flag in MechaCon NVRAM.  
  Valid values:
    - `jap` — Japanese
    - `eng` — English
    - `fre` — French
    - `spa` — Spanish
    - `ger` — German
    - `ita` — Italian
    - `dut` — Dutch
    - `por` — Portugese
    - `rus` — Russian
    - `kor` — Korean
    - `tch` — Traditional Chinese
    - `sch` — Simplified Chinese  

   Note that a list of available languages depends on your PS2 revision and model.  
   For example, Japanese consoles only support Japanese and English languages and will fallback to Japanese if any other language other than English is set.  
   OSDMenu MBR does **not** check whether your console actually supports the language.  

To disable configuration flags just for one boot path while keeping them for other paths, add a `-noflags` as the last argument.

### Supported paths

- `$HOSDSYS` — executes HOSDMenu or HDD-OSD from the following locations:
   - `hdd0:__system:pfs:osdmenu/hosdmenu.elf` — HOSDMenu
   - `hdd0:__system:pfs:osd100/hosdsys.elf` — HDD-OSD
   - `hdd0:__system:pfs:osd100/OSDSYS_A.XLF` — HDD-OSD (alternative path)
- `$PSBBN` — executes PlayStation Broadband Navigator from `hdd0:__system:pfs:p2lboot/osdboot.elf`
- `hdd?:<partition name>:PATINFO` — will boot using SYSTEM.CNF from the HDD partition attribute area on `hdd0` or `hdd1`
- `hdd?:<partition name>:pfs:<path to ELF>` — will boot the ELF from the PFS partition on `hdd0` or `hdd1`
- `ata?:<PATH>` — executes the ELF from the exFAT partition on `hdd0` or `hdd1`.
- `mc?:<PATH>` — executes the ELF from the memory card. Use `?` to make the MBR try both memory cards
- `xfrom:<PATH>` — executes the ELF from the XFROM device
- `cdrom` — boots the PS1/PS2 CD/DVD disc
- `dvd` — starts the DVD Player. 
 
PSX-only paths:
- `$XOSD` — executes PSX `xosdmain.elf` from `hdd0:__system:pfs:/BIEXEC-SYSTEM/xosdmain.elf`
- `$OSDMENU` — executes OSDMenu from `xfrom:/osdmenu/osdmenu.elf`

Note: Wildcard paths are **not** supported for `hdd` and `ata` paths.  
Be sure to use either `hdd0`/`ata0` or `hdd1`/`ata1`.

### SYSTEM.CNF extensions for partition attribute area (`:PATINFO`) paths

OSDMenu MBR supports additional SYSTEM.CNF arguments for `:PATINFO` paths.  
`SYSTEM.CNF` file from the partition attribute area can contain additional lines:
- `path` — custom ELF path (only `hdd0` and `mc?` paths are supported) 
- `arg` — custom argument to pass to the ELF file. More than one `arg` lines are supported.
- `skip_argv0` — if set to `1`, the target ELF will only receive argv built from `arg` lines (useful for running POPStarter).
- `titleid` — will use this title ID for the history file and visual game ID
- `nohistory` — if set to `1`, will disable writes to the history file while still showing game ID

### Embedded Neutrino GSM (eGSM)

OSDMenu MBR supports running disc-based PS2 games via the embedded [Neutrino GSM](../utils/egsm/) by automatically loading and applying the per-title options from `hdd0:__sysconf:pfs:osdmenu/OSDGSM.CNF` or `xfrom:/osdmenu/OSDGSM.CNF`.  

See the sample configuraton [here](../examples/OSDGSM.CNF) and [this](../utils/loader/README.md#egsm) README for more information on the eGSM argument format.

### ELF loader arguments

MBR supports the following ELF loader arguments for every defined path except special paths (`$HOSDSYS`, `$PSBBN`, `$XOSD`, `cdrom`, `dvd`):

- `-gsm=<options>` — runs the target ELF via the [embedded Neutrino GSM](../utils/egsm/).  
  See [this README](../utils/loader/README.md#egsm) for more information on the argument format.   
  Must always be the last argument.
- `-psxmode` — will **not** switch PSX into PS2 mode before running the target ELF
- `-dev9=` — will pass DEV9 shutdown flags to the loader. Supported values are:
  - `NICHDD` — will keep both the network adapter and HDD on
  - `NIC` — will keep only the network adapter on

These arguments must always come after any application-specific arguments.
