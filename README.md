# OSDMenu

Patches for OSDSYS and HDD OSD (Browser 2.0) based on Free McBoot 1.8.  

## Usage

### OSDMenu
1. Copy `osdmenu.elf` to `mc?:/BOOT/` or copy and paste `SYS_OSDMENU.psu` via wLaunchELF using `psuPaste`
   Copy DKWDRV to `mc?:/BOOT/DKWDRV.ELF` _(optional)_ 
2. Edit `mc?:/SYS-CONF/OSDMENU.CNF` [as you see fit](#osdmenucnf)
3. Configure PS2BBL to launch `mc?:/BOOT/osdmenu.elf` or launch it manually from anywhere

### OSDMenu as the System Update

You can install OSDMenu as the System Update instead of FMCB or PS2BBL to get faster boot times.  
The release archive contains the following files:
  - `kelf/OSDMENU.XLF` — encrypted and signed OSDMenu executable to be installed as `osd???.elf` or `osdmain.elf`
  - `kelf/osdmenu.icn` — OSDMenu icon for the PS2 Browser
  - `kelf/icon.sys` — icon manifest for the PS2 Browser

To install OSDMenu as the System Update, you can use the [KELFBinder](https://github.com/israpps/KELFBinder).  
Consult the KELFBinder documentation on how to use the KELFBinder to install your own payloads.

### HOSDMenu — OSDMenu for HDD OSD
1. Install HDD OSD 1.10U  
   Make sure HDD OSD binaries are installed into `hdd0:__system/osd100/` and `hosdsys.elf`/`OSDSYS_A.XLF` is present.  
   SHA-256 hashes of `hosdsys.elf`/`OSDSYS_A.XLF` known to work:
   - `acc905233f79678b9d7c1de99b0aee2409136197d13e7d78bf8978cd85b736ae` — original binary from the official HDD Utility Disc Version 1.10
   - `65360a6c210b36def924770f23d5565382b5fa4519ef0bb8ddf5c556531eec14` — cracked HDD OSD with 48-bit LBA support from the Sony Utility Disc Compilation 4 disc

   When using the unmodified binary on non-NTSC-U consoles, you will have to decrypt and re-encrypt the original binary with [`kelftool`](https://github.com/ps2homebrew/kelftool)
   to change the MagicGate region to 0xff (region free).
2. Copy `hosdmenu.elf` to `hdd0:__system/osdmenu/`  
   Copy DKWDRV to `hdd0:__system/osdmenu/DKWDRV.ELF` _(optional)_ 
3. Edit `hdd0:__sysconf/osdmenu/OSDMENU.CNF` [as you see fit](#osdmenucnf)
4. Configure your bootloader to launch `hdd0:__system/osdmenu/hosdmenu.elf` or launch it manually from anywhere  

### OSDMenu MBR

You can install OSDMenu MBR into the `__mbr` partition of your HDD for faster HOSDMenu boot times and improved PSBBN support.  
The release archive contains the following files:
  - `osdmbr/OSDMBR.XLF` — encrypted and signed OSDMenu MBR executable to be installed as `__mbr`
  - `osdmbr/osdmbr-installer.elf` — installer that will automatically install the MBR, enable the MBR boot and copy the example configuration file
  - `osdmbr/payloads/` — encrypted binaries for advanced users

See the MBR [README](mbr/README.md) for more details.  


## Key differences from FMCB 1.8:
- All initialization code is removed in favor of using a separate bootloader to start the patcher (e.g. [PS2BBL](https://github.com/israpps/PlayStation2-Basic-BootLoader))
- USB support is dropped from the patcher, so only memory cards are checked for `OSDMENU.CNF`
- No ESR support
- No support for launching ELFs by holding a gamepad button
- ELF paths are not checked by the patcher, so every named entry from FMCB config file is displayed in hacked OSDSYS menu
- A separate launcher is used to launch menu entries
- CD/DVD support was extended to support skipping PS2LOGO, mounting VMCs on MMCE devices, showing visual GameID for PixelFX devices and booting DKWDRV for PS1 discs
- "Unlimited" number of paths for each entry
- Support for 1080i and 480p (as line-doubled 240p) video modes
- Support for "protokernel" systems (SCPH-10000, SCPH-15000) ported from Free McBoot 1.9 by reverse-engineering
- Support for launching applications from the memory card browser
- Support for setting PS1 driver options on every boot
- Support for HDD OSD 1.10U

## Patcher

This is a slimmed-down and refactored version of OSDSYS patches from FMCB 1.8 for modern PS2SDK with some new patches sprinkled in.
It patches the OSDSYS/HDD OSD binary and applies the following patches:
- Custom OSDSYS menu with up to 200 entries
- Infinite scrolling
- Custom button prompts and menu header
- Automatic disc launch bypass
- Force GS video mode to PAL, NTSC, 1080i or line-doubled 480p (with half the vertical resolution).  
  _Due to how to OSDSYS renders everything, "true" 480p can't be implemented easily_
- HDD update check bypass
- Integrated Neutrino GSM
- Override PS1 and PS2 disc launch functions with the launcher, bringing the following features to OSDSYS/HDD-OSD:
  - Skip the PlayStation 2 logo
  - Display the visual Game ID for the PixelFX RetroGM
  - Run disc-based games via the embedded Neutrino GSM (eGSM)
  - Run PS1 discs via the PS1 Video Mode Negator or DKWDRV
- Additional system information in version submenu (Video mode, ROM version, EE, GS and MechaCon revision)  
- Set PS1 driver options to values from `OSDMENU.CNF` on every boot

**OSDMenu**:
- The OSDMenu configuration file can be embedded into the patcher at compile time for memory card-independent setups and faster boot times
- Launch SAS-compatible applications from the memory card browser if directory name is   
  `BOOT`, `<3-letter SAS prefix>_<appname>` or if file name ends with `.ELF` or `.elf`.  
  This patch swaps around the "Enter" and "Options" menus and substitutes file properties submenu with the launcher.  
  To launch an app, just press "Enter" after selecting the app icon.  
  To copy or delete the save file, just use the triangle button.  

**HOSDMenu**:
- Boot custom payload when the Cross button is pressed during the initialization (see [here](#running-as-__mbr)).  
- Launch SAS-compatible applications and ELF files from directories in the `hdd0:__common` partition or the memory card browser
  if directory name is `BOOT`, `<3-letter SAS prefix>_<appname>` or if file name ends with `.ELF` or `.elf`.  
  To launch an app, just press "Enter" after selecting the app icon.
- ATAD driver is replaced to bypass security checks and support drives larger than 1TB.
- HDD partitions that end with `.HIDDEN` are not shown in the HDD Browser.  
    
  Note that HDD OSD will not see more than 1048448 MB. For larger drives, [APA Jail](https://www.psx-place.com/threads/apa-jail.34847/) is recommended.  
  You can also check out [PSBBN Definitive English Patch](https://github.com/CosmicScale/PSBBN-Definitive-English-Patch) for more automated APA Jail experience and easy-to-use HDD OSD+Broadband Navigator setup.

  HOSDMenu will skip the full IOP initialization when it receives `-mbrboot` as the last argument (`argv[argc - 1]`), improving boot times when running from a compatible `__mbr`.

Patches not supported/limited on protokernel systems:
- Automatic disc launch bypass
- Button prompt customization
- PAL video mode

**OSDMenu** version of the patcher reads settings from `mc?:/SYS-CONF/OSDMENU.CNF` (if the config file is not embedded) and patches the `rom0:OSDSYS` binary.  
**HOSDMenu** version reads settings from `hdd0:__sysconf/osdmenu/OSDMENU.CNF` and patches `hdd0:__system/osd100/OSDSYS_A.XLF` or `hdd0:__system/osd100/hosdsys.elf`

### Configuration

See the list for supported `OSDMENU.CNF` options [here](#osdmenucnf).  
OSDMenu will run the embedded launcher and pass the menu index to it for every menu item and disc launch.

The embedded Neutrino GSM (eGSM) can be configured using the `OSDGSM.CNF` file, see the additional information [here](#osdgsmcnf).  

## OSDMenu MBR

OSDMenu comes with the fully-featured MBR that supports running HOSDMenu, HDD-OSD and PSBBN natively.  
It also supports running arbitrary paths from the HDD and memory cards.  
See the MBR [README](mbr/README.md) for more details.

## Launcher

A fully-featured ELF launcher that handles launching ELFs and CD/DVD discs.  
Supports passing arbitrary arguments to an ELF and can also be used standalone.

Supported paths are:
- `mmce?:` — MMCE devices. Can be `mmce0`, `mmce1` or `mmce?`
- `mc?:` — Memory Cards. Can be `mc0`, `mc1` or `mc?`
- `mass:` and `usb:` — USB devices (supported via BDM)
- `ata:` — internal exFAT-formatted HDD (supported via BDM)
- `mx4sio:` — MX4SIO (supported via BDM)
- `ilink:` — i.Link mass storage (supported via BDM)
- `udpbd:` — UDPBD (supported via BDM)
- `hdd0:` — internal APA-formatted HDD
- `rom?:` — ROM binaries
- `cdrom` — CD/DVD discs
- `osdm` — special path for the patcher

Device support can be enabled and disabled by changing build-time configuration options (see [Makefile](launcher/Makefile))

### Global arguments

The launcher supports the following global arguments:

- `-gsm=<>` — runs the target ELF via the [embedded Neutrino GSM](utils/egsm/).  
  See [this README](utils/loader/README.md#egsm) for more information on the argument format.   
  Must always be the last argument. Does not apply to `rom?:` paths.
- `-appid` — enables visual Game ID for applications. The ID is generated from the ELF name (up to 11 characters).

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
`arg` — arguments that will be passed to the ELF file 

## OSDMENU.CNF

Most of `OSDMENU.CNF` settings are directly compatible with those from FMCB 1.8 `FREEMCB.CNF`.

### Character limits

OSDMenu supports up to 200 custom menu entries, each up to 79 characters long.  
Note that left and right cursors are limited to 19 characters and top and bottom delimiters are limited to 79 characters.  
DKWDRV and custom payload paths are limited to 49 characters.

### Configuration options

1. `OSDSYS_video_mode` — force OSDSYS mode. Valid values are `AUTO`, `PAL`, `NTSC`, `480p` or `1080i`
2. `OSDSYS_scroll_menu` — enables or disables infinite scrolling
3. `OSDSYS_menu_x` — menu X center coordinate
4. `OSDSYS_menu_y` — menu Y center coordinate
5. `OSDSYS_enter_x` — `Enter` button X coordinate (at main OSDSYS menu)
6. `OSDSYS_enter_y` — `Enter` button Y coordinate (at main OSDSYS menu)
7. `OSDSYS_version_x` — `Version` button X coordinate (at main OSDSYS menu)
8. `OSDSYS_version_y` — `Version` button Y coordinate (at main OSDSYS menu)
9. `OSDSYS_cursor_max_velocity` — max cursor speed
10. `OSDSYS_cursor_acceleration` — cursor speed
11. `OSDSYS_left_cursor` — left cursor text
12. `OSDSYS_right_cursor` — right cursor text
13. `OSDSYS_menu_top_delimiter` — top menu delimiter text
14. `OSDSYS_menu_bottom_delimiter` — bottom menu delimiter text
15. `OSDSYS_num_displayed_items` — the number of menu items displayed
16. `OSDSYS_Skip_Disc` — enables/disables automatic CD/DVD launch
17. `OSDSYS_Skip_Logo` — enables/disables SCE logo (also needs `OSDSYS_Skip_Disc` to be disabled to actually show the logo)
18. `OSDSYS_Inner_Browser` — enables/disables going to the Browser after launching OSDSYS
19. `OSDSYS_selected_color` — color of selected menu entry
20. `OSDSYS_unselected_color` — color of unselected menu entry
21. `name_OSDSYS_ITEM_???` — menu entry name
22. `path?_OSDSYS_ITEM_???` — path to ELF. Also supports the following special paths: `cdrom`, `OSDSYS`, `POWEROFF`

New to OSDMenu/HOSDMenu:

23. `arg_OSDSYS_ITEM_???` — custom argument to be passed to the ELF. Each argument needs a separate entry.
24. `cdrom_skip_ps2logo` — enables or disables running discs via `rom0:PS2LOGO`. Useful for MechaPwn-patched consoles.
25. `cdrom_disable_gameid` — disables or enables visual Game ID
26. `cdrom_use_dkwdrv` — enables or disables launching DKWDRV for PS1 discs
27. `ps1drv_enable_fast` — will enable fast disc speed for PS1 discs when not using DKWDRV
28. `ps1drv_enable_smooth` — will enable texture smoothing for PS1 discs when not using DKWDRV
29. `ps1drv_use_ps1vn` — will run PS1DRV using the PS1DRV Video Mode Negator
30. `app_gameid` — if enabled, visual Game ID will be displayed for ELF applications launched from OSDMenu. The ID is generated from the ELF name (up to 11 characters).

Options exclusive to OSDMenu:

31. `path_DKWDRV_ELF` — custom path to DKWDRV.ELF. The path MUST be on the memory card

## OSDGSM.CNF

OSDMenu supports running disc-based PS2 games via the embedded [Neutrino GSM](utils/egsm/).

**OSDMenu** loads the per-title options from `mc?:/SYS-CONF/OSDGSM.CNF`.  
**HOSDMenu** loads the per-title options from `hdd0:__sysconf/osdmenu/OSDGSM.CNF`, with fallback to `mc?:/SYS-CONF/OSDGSM.CNF` if the file on the HDD doesn't exist.

See the sample configuraton [here](examples/OSDGSM.CNF) and [this](utils/loader/README.md#egsm) README for more information on the argument format.

## Credits

- Everyone involved in developing the original Free MC Boot and OSDSYS patches, especially Neme and jimmikaelkael
- Julian Uy for mapping out significant parts of HDD OSD for [osdsys_re](https://github.com/ps2re/osdsys_re) project
- [TonyHax International](https://github.com/alex-free/tonyhax) developers for PS1 game ID detection for generic executables.
- Maximus32 for creating the [`smap_udpbd` module](
https://github.com/rickgaiser/neutrino) and Neutrino GSM
- Matías Israelson for making [PS2BBL](https://github.com/israpps/PlayStation2-Basic-BootLoader)
- CosmicScale for [RetroGEM Disc Launcher](https://github.com/CosmicScale/Retro-GEM-PS2-Disc-Launcher)
- Ripto for creating the OSDMenu Browser icons and Yornn for collecting all files required for the PSU package