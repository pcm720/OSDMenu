// Defines paths used by both the patcher and the launcher
#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_

//
// OSDMenu paths
//

// All files must be placed on the memory card.
// mc? paths are also supported.
#ifndef CONF_PATH
#define CONF_PATH "mc0:/SYS-CONF/OSDMENU.CNF"
#endif

#ifndef DKWDRV_PATH
#define DKWDRV_PATH "mc0:/BOOT/DKWDRV.ELF"
#endif

//
// HOSDMenu paths
//

// Partition containing OSDMENU.CNF
#define HOSD_CONF_PARTITION "hdd0:__sysconf"

// Partition containing DKWDRV
#define HOSD_SYS_PARTITION "hdd0:__system"

// Path relative to HOSD_SYS_PARTITION root
#ifndef HOSDMBR_CONF_PATH
#define HOSD_PFS_PATH "/osdmenu/hosdmenu.elf"
#endif

// Path relative to HOSD_CONF_PARTITION root
#ifndef HOSD_CONF_PATH
#define HOSD_CONF_PATH "/osdmenu/OSDMENU.CNF"
#endif

// Path relative to HOSD_CONF_PARTITION root
#ifndef HOSDMBR_CONF_PATH
#define HOSDMBR_CONF_PATH "/osdmenu/OSDMBR.CNF"
#endif

// Full path to DKWDRV, including the partition
#ifndef HOSD_DKWDRV_PATH
#define HOSD_DKWDRV_PATH HOSD_SYS_PARTITION ":pfs:/osdmenu/DKWDRV.ELF"
#endif

//
// HDD-OSD and PSBBN paths, relative to SYSTEM_PARTITION root
//

// System partition
#define SYSTEM_PARTITION "hdd0:__system"

// HDD-OSD
#define HOSDSYS_PFS_PATH_1 "/osd100/hosdsys.elf"
#define HOSDSYS_PFS_PATH_2 "/osd100/OSDSYS_A.XLF"

// PSBBN launcher path
#define OSDBOOT_PFS_PATH "/p2lboot/osdboot.elf"

#endif
