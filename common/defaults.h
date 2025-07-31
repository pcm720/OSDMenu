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

// Path relative to HOSD_CONF_PARTITION root
#ifndef HOSD_CONF_PATH
#define HOSD_CONF_PATH "/OSDMENU/OSDMENU.CNF"
#endif

// Full path to DKWDRV, including the partition
#ifndef HOSD_DKWDRV_PATH
#define HOSD_DKWDRV_PATH HOSD_SYS_PARTITION ":pfs:/osdmenu/DKWDRV.ELF"
#endif

#endif
