# OSDMenu

Patches for OSDSYS and HDD OSD (Browser 2.0) based on Free McBoot 1.8.  

## Usage

### OSDMenu
1. Copy `osdmenu.elf` to `mc?:/BOOT/` or copy and paste `SYS_OSDMENU.psu` via wLaunchELF using `psuPaste`
   Copy DKWDRV to `mc?:/BOOT/DKWDRV.ELF` _(optional)_ 
2. Edit `mc?:/SYS-CONF/OSDMENU.CNF` [as you see fit](patcher/README.md#osdmenucnf)
3. Configure PS2BBL to launch `mc?:/BOOT/osdmenu.elf` or launch it manually from anywhere

### OSDMenu as the System Update

You can install OSDMenu as the System Update instead of FMCB or PS2BBL to get faster boot times.  
To install OSDMenu as the System Update, you can use [KELFBinder](https://github.com/israpps/KELFBinder).  

The release archive contains ready-to-use KELFBinder install script and directory structure:
  - `patcher/kelfbinder/EXTINST.lua` — custom installation script that installs OSDMenu as the system update and copies wLaunchELF from KELFBinder release into `BOOT/BOOT.ELF`
  - `patcher/kelfbinder/KELF/SYSTEM.XLF` — encrypted and signed OSDMenu executable to be installed as `osd???.elf` or `osdmain.elf`
  - `patcher/kelfbinder/ASSETS/osdmenu.icn` and `*.sys` files — OSDMenu icon assets for the PS2 Browser
  - `patcher/kelfbinder/ASSETS/SYS-CONF/OSDMENU.CNF`, `icon.sys` and `list.icn` — the [example config file](examples/OSDMENU.CNF) and `SYS-CONF` icons

Copy the contents of the `patcher/kelfbinder` directory into KELFBinder's `INSTALL` directory, replacing all existing files.  
Consult the KELFBinder documentation on how to use KELFBinder to install the system update on your memory card.

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
3. Edit `hdd0:__sysconf/osdmenu/OSDMENU.CNF` [as you see fit](patcher/README.md#osdmenucnf)
4. Configure your bootloader to launch `hdd0:__system/osdmenu/hosdmenu.elf` or launch it manually from anywhere  

### OSDMenu MBR

You can install OSDMenu MBR into the `__mbr` partition of your HDD for faster HOSDMenu boot times and improved PSBBN support.  
The release archive contains the following files:
  - `osdmbr/OSDMBR.XLF` — encrypted and signed OSDMenu MBR executable to be installed as `__mbr`
  - `osdmbr/osdmbr-installer.elf` — installer that will automatically install the MBR, enable the MBR boot and copy the example configuration file
  - `osdmbr/payloads/` — encrypted binaries for advanced users

See the MBR [README](mbr/README.md) for more details. 

### OSDMenu Launcher

OSDMenu Launcher can be used as a standalone forwarder for launching applications from any supported device.
The release archive contains the following files:
  - `launcher/launcher.elf`--- standalone OSDMenu Launcher 
  - `launcher/OSDMBR.XLF` — encrypted and signed OSDMenu Launcher executable for PATINFO injection

See the launcher [README](launcher/README.md) for more details. 

## Key differences from FMCB 1.8:
- All OSD initialization code is removed
- USB support is dropped from the patcher, so only memory cards are checked for `OSDMENU.CNF`
- No ESR support
- No support for launching ELFs by holding a gamepad button
- ELF paths are not checked by the patcher, so every named entry from FMCB config file is displayed in hacked OSDSYS menu
- Support for launching applications from MMCE, MX4SIO and APA- and exFAT-formatted HDDs
- CD/DVD support was extended to support skipping PS2LOGO, mounting VMCs on MMCE devices, showing visual GameID for PixelFX devices and booting DKWDRV for PS1 discs
- Integrated Neutrino GSM for disc games and applications
- "Unlimited" number of paths for each entry
- Support for 1080i and 480p (as line-doubled 240p) video modes
- Support for "protokernel" systems (SCPH-10000, SCPH-15000) ported from Free McBoot 1.9 by reverse-engineering
- Support for launching applications from the memory card browser
- Support for setting PS1 driver options on every boot
- Support for HDD OSD 1.10U

## Configuration

### OSDMenu and HOSDMenu

See the patcher [README](patcher/README.md) for more details.

### OSDMenu MBR

OSDMenu comes with the fully-featured MBR that supports running HOSDMenu, HDD-OSD and PSBBN natively.  
It also supports running arbitrary paths from the HDD and memory cards.  
See the MBR [README](mbr/README.md) for more details.

### OSDMenu Launcher

OSDMenu comes with the fully-featured launcher that supports running applications from all devices supported by homebrew drivers.  
See the launcher [README](launcher/README.md) for more details.

## Credits

- Everyone involved in developing the original Free MC Boot and OSDSYS patches, especially Neme and jimmikaelkael
- Julian Uy for mapping out significant parts of HDD OSD for [osdsys_re](https://github.com/ps2re/osdsys_re) project
- [TonyHax International](https://github.com/alex-free/tonyhax) developers for PS1 game ID detection for generic executables.
- Rick Gaiser/Maximus32 for creating [Neutrino](https://github.com/rickgaiser/neutrino), parts of which are used by OSDMenu 
- Matías Israelson for creating [PS2BBL](https://github.com/israpps/PlayStation2-Basic-BootLoader)
- CosmicScale for [RetroGEM Disc Launcher](https://github.com/CosmicScale/Retro-GEM-PS2-Disc-Launcher), [PSBBN Definitive English Patch](https://github.com/CosmicScale/PSBBN-Definitive-English-Patch) and extensive testing  
- Ripto for creating OSDMenu Browser icons and Yornn for collecting all files required for the PSU package    
- Alex Parrado for creating [SoftDev2 installer](https://github.com/parrado/SoftDev2)
- [R3Z3N/Saildot4K](https://github.com/saildot4k) for testing OSDMenu with various modchips, Crystal Chip PBT script and suggestions on documentation and release packaging improvements