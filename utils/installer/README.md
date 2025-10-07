# OSDMenu MBR installer

This is a simple application for installing the [OSDMenu MBR](../../mbr/).  
Based on the [SoftDev2 installer](https://github.com/parrado/SoftDev2) by Alex Parrado.

Note that this installer doesn't set the HDD boot flags.

Requires properly configured `kelftool` to be present in the `./kelftool/` directory

## Usage instructions

1. Get the [kelftool](https://github.com/ps2homebrew/kelftool), build it and place the `kelftool` binary into the `./kelftool` directory
2. Add the keys
3. Run `make`. The `Makefile` will:
   - Build the OSDMenu MBR
   - Embed the resulting binary into the installer
4. Run `osdmbr_installer.elf` on your PS2.  
   This will install the MBR and copy the [MBR config file](config/osdmbr.cnf) to `hdd0:__sysconf/osdmenu/OSDMBR.CNF` if it doesn't exist
