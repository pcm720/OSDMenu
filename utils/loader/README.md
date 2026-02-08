# Embeddable ELF loader

Loads an ELF/KELF file from the path specified in argv[0].  
Loads in the `0x084000 - 0x0A0000` memory region to avoid possible conflicts with some unpacked homebrew and OSDMenu eGSM and PS1VN.

## Advanced features

### Loader flags

The loader's behavior can be altered by an optional last command-line argument (`argv[argc-1]`).  
This argument should begin with `-la=`, followed by one or more letters that modify the loader's behavior:
- `R` — reset IOP
- `N` — put the HDD into idle mode and keep the rest of DEV9 powered on (`HDDUNITPOWER = NIC`)
- `D` — keep both the HDD and DEV9 powered on (`HDDUNITPOWER = NICHDD`)
- `I` — the `argv[argc-2]` argument contains IOPRP image path (for HDD, the path must be a `pfs:` path on the same partition as the ELF file)
- `E` — the `argv[argc-2]` argument contains ELF memory location to use instead of `argv[0]`
- `A` — do not pass `argv[0]` to the target ELF and start with `argv[1]`
- `G` — force video mode via eGSM. The `argv[argc-2]` argument contains eGSM arguments  
- `P` — patch PS2LOGO to selected region if argv[0] is `rom0:PS2LOGO`. The `argv[argc-2]` argument should be `P` for PAL, `N` for NTSC  

Note:
  - `D` and `N` are mutually exclusive; if both are specified, only the last one will take effect.
  - When using arguments that require `argv[argc-2]` together, the arguments are interpreted in the order they appear.
    For example:
    - If specifying `IE`, `argv[argc-2]` is treated as the IOPRP path, and `argv[argc-3]` as the ELF path.
    - With `EI`, `argv[argc-2]` is treated as the ELF memory location, and `argv[argc-3]` as the IOPRP path.

The syntax for specifying a memory location is `mem:<8-char address in HEX>:<8-char file size in HEX>`

### eGSM

Based on [Neutrino GSM](https://github.com/rickgaiser/neutrino) by Rick Gaiser.  
This loader supports running the target executable via Neutrino GSM, forcing the application video mode.

eGSM options are inherited from Neutrino GSM and defined in the `v:c` format, where
- `v` — video mode.  
  - Empty value — don't force (default)  (480i/576i)
  - `fp1` — force progressive scan (240p/288p)
  - `fp2` — force progressive scan (480p/576p)
  - `1080ix1` — force 1080i, width/height x1
  - `1080ix2` — force 1080i, width/height x2
  - `1080ix3` — force 1080i, width/height x3
- `c` — compatibility mode
  - Empty value — no compatibility mode (default)
  - `1` — field flipping type 1 (GSM/OPL)
  - `2` — field flipping type 2
  - `3` — field flipping type 3

Examples:
- `fp2`     - recommended mode
- `fp2:1`   - recommended mode with compatibility mode 1
- `1080ix2` - force 1080i with x2 scaling