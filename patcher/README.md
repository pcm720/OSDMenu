# OSDMenu Patcher

This is a slimmed-down and refactored version of OSDSYS patches from FMCB 1.8 for modern PS2SDK with some new patches sprinkled in.
It patches the OSDSYS/HDD OSD binary and applies the following patches:
- Custom OSDSYS menu with up to 200 entries
- Infinite scrolling
- Custom button prompts and menu header
- Automatic disc launch bypass
- Force GS video mode to PAL, NTSC, 1080i or line-doubled 480p (with half the vertical resolution).  
  _Due to how to OSDSYS renders everything, "true" 480p can't be implemented easily_
- HDD update check bypass
- Override PS1 and PS2 disc launch functions with the launcher, bringing the following features to OSDSYS/HDD-OSD:
  - Skip the PlayStation 2 logo
  - Patch the PlayStation 2 logo to work with all disc regions
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

## OSDGSM.CNF

OSDMenu supports running disc-based PS2 games via the embedded [Neutrino GSM](../utils/egsm/).

**OSDMenu** loads the per-title options from `mc?:/SYS-CONF/OSDGSM.CNF`.  
**HOSDMenu** loads the per-title options from `hdd0:__sysconf/osdmenu/OSDGSM.CNF`, with fallback to `mc?:/SYS-CONF/OSDGSM.CNF` if the file on the HDD doesn't exist.

See the sample configuraton [here](../examples/OSDGSM.CNF) and [this](../utils/loader/README.md#egsm) README for more information on the argument format.

## OSDMENU.CNF

Most of `OSDMENU.CNF` settings are directly compatible with those from FMCB 1.8 `FREEMCB.CNF`.

### Character limits

OSDMenu supports up to 200 custom menu entries, each up to 79 characters long.  
Note that left and right cursors are limited to 19 characters and top and bottom delimiters are limited to 79 characters.  
DKWDRV and custom payload paths are limited to 49 characters.

### Configuration options

#### OSDSYS behavior modifiers
1. `OSDSYS_video_mode` — force OSDSYS mode. Valid values are `AUTO`, `PAL`, `NTSC`, `480p` or `1080i`
2. `OSDSYS_Skip_Disc` — enables/disables automatic CD/DVD launch
3. `OSDSYS_Skip_Logo` — enables/disables SCE logo (also needs `OSDSYS_Skip_Disc` to be disabled to actually show the logo)
4. `OSDSYS_Inner_Browser` — enables/disables going to the Browser after launching OSDSYS

#### OSDSYS custom menu options
5. `OSDSYS_custom_menu` — enables or disables custom menu
6. `OSDSYS_scroll_menu` — enables or disables infinite scrolling in custom menu
7. `OSDSYS_menu_x` — menu X center coordinate
8. `OSDSYS_menu_y` — menu Y center coordinate
9. `OSDSYS_enter_x` — `Enter` button X coordinate (at main OSDSYS menu)
10. `OSDSYS_enter_y` — `Enter` button Y coordinate (at main OSDSYS menu)
11. `OSDSYS_version_x` — `Version` button X coordinate (at main OSDSYS menu)
12. `OSDSYS_version_y` — `Version` button Y coordinate (at main OSDSYS menu)
13. `OSDSYS_cursor_max_velocity` — max cursor speed
14. `OSDSYS_cursor_acceleration` — cursor speed
15. `OSDSYS_left_cursor` — left cursor text
16. `OSDSYS_right_cursor` — right cursor text
17. `OSDSYS_menu_top_delimiter` — top menu delimiter text
18. `OSDSYS_menu_bottom_delimiter` — bottom menu delimiter text
19. `OSDSYS_num_displayed_items` — the number of menu items displayed
20. `OSDSYS_selected_color` — color of selected menu entry
21. `OSDSYS_unselected_color` — color of unselected menu entry
22. `name_OSDSYS_ITEM_???` — menu entry name
23. `path?_OSDSYS_ITEM_???` — path to ELF. Also supports the following special paths: `cdrom`, `OSDSYS`, `POWEROFF`
24. `arg_OSDSYS_ITEM_???` — custom argument to be passed to the ELF. Each argument needs a separate entry.

#### Disc/application launch modifiers
25. `cdrom_skip_ps2logo` — enables or disables running discs via `rom0:PS2LOGO`
26. `cdrom_disable_gameid` — disables or enables visual Game ID
27. `cdrom_use_dkwdrv` — enables or disables launching DKWDRV for PS1 discs
28. `ps1drv_enable_fast` — will enable fast disc speed for PS1 discs when not using DKWDRV
29. `ps1drv_enable_smooth` — will enable texture smoothing for PS1 discs when not using DKWDRV
30. `ps1drv_use_ps1vn` — will run PS1DRV using the PS1DRV Video Mode Negator
31. `app_gameid` — if enabled, visual Game ID will be displayed for ELF applications launched from OSDMenu. The ID is generated from the ELF name (up to 11 characters).
32. `path_DKWDRV_ELF` — custom path to DKWDRV.ELF (exclsuive to OSDMenu). The path MUST be on the memory card, the default value is `mc?:/BOOT/DKWDRV.ELF`

To add a custom separator to the menu, add a `name_OSDSYS_ITEM_???` entry that starts with `$!`.  
This will make the entry inactive, but still show it in the OSD without the `$!` prefix.

By default, OSDMenu uses custom menu coordinates to make the menu appear in the center of the screen.  
To get the original OSDSYS look, set the following values in `OSDMENU.CNF`:
```
OSDSYS_menu_x = 430
OSDSYS_menu_y = 110
OSDSYS_enter_x = -1
OSDSYS_enter_y = -1
OSDSYS_version_x = -1
OSDSYS_version_y = -1
```
