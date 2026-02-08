
-- IF YOU WANT TO DISABLE INSTALLATION OF EXTRA FILES press L1 on main menu (R1 Enables them back)

SYSUPDATE_ICON_SYS = "osdmenu.icn" -- icon files for memory card update
SYSUPDATE_ICON_SYS_LOCATION = "INSTALL/ASSETS/" --where to find the icon file for memory card update
SYSUPDATE_ICON_SYS_RES = SYSUPDATE_ICON_SYS_LOCATION..SYSUPDATE_ICON_SYS

--- installation table for memory card.
MC_INST_TABLE = {
  source = {
    "INSTALL/ASSETS/SYS-CONF/OSDMENU.CNF",
    "INSTALL/ASSETS/SYS-CONF/icon.sys",
    "INSTALL/ASSETS/SYS-CONF/list.icn"
  }, --- holds file locations relative to KELFBinder CWD.
  target = {
    "SYS-CONF/OSDMENU.CNF",
    "SYS-CONF/icon.sys",
    "SYS-CONF/list.icn"
  }, --- contains installation paths ignoring device (eg: instead of `mc0:/A/BOOT.ELF` use `A/BOOT.ELF`)
  dirs = {
    "SYS-CONF"
  } --- contains a list of directory names to be created before writing files to target
}

--- installation table for HardDrive
HDD_INST_TABLE = {
  source = { --- holds file locations relative to KELFBinder CWD.
    SYSUPDATE_HDD_MAIN,
    "INSTALL/KELF/FSCK.XLF"
  },
  target = { --- contains fully qualified HDD paths, so installation routine has all the needed info (partition name, and PFS path)
    "hdd0:__system:pfs:/osd/osdmain.elf",
    "hdd0:__system:pfs:/fsck/fsck.elf"
  },
  dirs = { --- contains a list of directory names to be created before writing files to target
    "hdd0:__system:pfs:/osd/",
    "hdd0:__system:pfs:/fsck/"
  },
}

DVDPL_INST_TABLE = {
  source = {}, --- holds file locations relative to KELFBinder CWD.
  target = {}, --- contains installation paths ignoring device (eg: instead of `mc0:/A/BOOT.ELF` use `A/BOOT.ELF`)
  dirs = {} --- contains a list of directory names to be created before writing files to target
}

--- Here we declare memory card installation folders (beyond the system update folder)
-- Update_InstTable("INSTALL/ASSETS/SYS-CONF", "SYS-CONF", MC_INST_TABLE)
-- Update_InstTable("INSTALL/ASSETS/APPS",     "APPS",     MC_INST_TABLE)
Update_InstTable("INSTALL/ASSETS/BOOT",     "BOOT",     MC_INST_TABLE)

--- HDD full paths look like this: `hdd0:PARTITION:pfs:PATH_INSIDE_PARTITON`
Update_InstTable("INSTALL/ASSETS/PS2BBL-HDD", "hdd0:__sysconf:pfs:/PS2BBL",   HDD_INST_TABLE)
Update_InstTable("INSTALL/ASSETS/APPS-HDD",   "hdd0:__common:pfs:/APPS",      HDD_INST_TABLE)
Update_InstTable("INSTALL/ASSETS/BOOT-HDD",   "hdd0:__sysconf:pfs:/BOOT",     HDD_INST_TABLE)
Update_InstTable("INSTALL/ASSETS/FSCK",       "hdd0:__system:pfs:/fsck/lang", HDD_INST_TABLE)

--- The DVDPlayer table is a special case, here, we do not declare a destination folder
--- because that will be determined by user input when the program rums
--- hence why we pass an empty string here
Update_InstTable("INSTALL/ASSETS/DVDPLAYER_FILES", "", DVDPL_INST_TABLE)


---DEBUG: here we list what will be installed
System.log("MC installation table:\n")
for x = 1, #MC_INST_TABLE.source do
  System.log(string.format("\t[%s] > [%s]\n", MC_INST_TABLE.source[x], MC_INST_TABLE.target[x]))
end

System.log("HDD installation table:\n")
for x = 1, #HDD_INST_TABLE.source do
  System.log(string.format("\t[%s] > [%s]\n", HDD_INST_TABLE.source[x], HDD_INST_TABLE.target[x]))
end

System.log("DVDPlayer installation table:\n")
for x = 1, #DVDPL_INST_TABLE.source do
  System.log(string.format("\t[%s] > [%s]\n", DVDPL_INST_TABLE.source[x], DVDPL_INST_TABLE.target[x]))
end
