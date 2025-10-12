# Embeddable ELF loader

Loads an ELF file from the path specified in argv[0].  

## Advanced features

### Loader flags

The loader's behavior can be altered by an optional last command-line argument (`argv[argc-1]`).  
This argument should begin with `-la=`, followed by one or more letters that modify the loader's behavior:
- `R` — reset IOP
- `N` — put the HDD into idle mode and keep the rest of DEV9 powered on (`HDDUNITPOWER = NIC`)
- `D` — keep both the HDD and DEV9 powered on (`HDDUNITPOWER = NICHDD`)
- `I` — the `argv[argc-2]` argument contains IOPRP image path (for HDD, the path must be a pfs: path on the same partition as the ELF file)
- `E` — the `argv[argc-2]` argument contains ELF memory location to use instead of `argv[0]`
- `G` — force video mode via eGSM. The `argv[argc-2]` argument contains eGSM arguments
Note:
  - `D` and `N` are mutually exclusive; if both are specified, only the last one will take effect.
  - When using arguments that require `argv[argc-2]` together, the arguments are interpreted in the order they appear.
    For example:
    - If specifying `IE`, `argv[argc-2]` is treated as the IOPRP path, and argv[argc-3] as the ELF path.
    - With `EI`, `argv[argc-2]` is treated as the ELF memory location, and argv[argc-3] as the IOPRP path.

The syntax for specifying a memory location is `mem:<8-char address in HEX>:<8-char file size in HEX>`

### eGSM

This loader supports running the target executable via the embedded [Neutrino GSM](../egsm/), forcing the application video mode.

eGSM options are inherited from Neutrino GSM and defined in the `x:y:z` format, where
- `x` — interlaced field mode, used when a full-height buffer is used by the title for displaying.  
  - Empty value — don't force (default)  (480i/576i)
  - `fp` — force progressive scan (480p/576p)
- `y` — interlaced frame mode, used when a half-height buffer is used by the title for displaying.  
  - Empty value — don't force (default)  (480i/576i)
  - `fp1` — force progressive scan (240p/288p)
  - `fp2` — force progressive scan (480p/576p line doubling)
- `z` — compatibility mode
  - Empty value — no compatibility mode (default)
  - `1` — field flipping type 1 (GSM/OPL)
  - `2` — field flipping type 2
  - `3` — field flipping type 3

Examples:
- `fp`       - force progressive for full-height buffer titles
- `fp::1`    - force progressive for full-height buffer titles with compatibility mode 1
- `:fp2:1`   - force progressive 480p/576p for half-height buffer titles with compatibility mode 1
- `fp:fp2:2` - force progressive 480p/576p with compatibility mode 2