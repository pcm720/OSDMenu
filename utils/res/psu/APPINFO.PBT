# SAS (Save Application Support) compliant BM Script
# Due to wildcard bug on memory card and mmce, bootdevice:/BM/SCRIPTS/DEFINEDROOTFOLDER.PBT must exist to work. Update as needed

# Change this information to describe the application and where it should be ran from for memcard (SAS) or other devices (SAS_NON_MC)
# SAS is the App folder name. SAS_NON_MC defines if app is in root of non-memcard device (non_mc:/$SAS$) or APP folder (non_mc:/APPS/$SAS$)
# This is so that you do not have to edit any info below line 28.
#
# Some devices are case sensitive! Make sure your case matches!
#
# Source: https://github.com/pcm720/OSDMenu
SET "TITLE" "[SYS] OSDMenu"
SET "VERSION" "1.0.0"
SET "AUTHOR" "pcm720"
SET "DESC" "Hacked OSDSYS based off FMCB 1.8 however with many enhancements such as more device support, GameID etc."
SET "ELF" "osdmenu.elf"
SET "SAS" "SYS_OSDMENU"
# Comment out 1 of the 2 lines below. If app is in non-mc:/$SAS$ comment out the first, if in non-mc:/APPS/$SAS$ comment out the second
# SET "SAS_NON_MC" "$SAS$"
SET "SAS_NON_MC" "APPS/$SAS$"
#
#

# Do not change these 2 lines!
GOTO "$ARG1$"
RETURN -1

# Used for Autoboot Labels
:LABEL_NAME
    ADDWIDGET "LABEL" "$ARG2$$TITLE$ v$VERSION$"
    EXIT 0

:QUERY
    ADDWIDGET "CALL" "$TITLE$" "$BM.TXT_VERSION$: $VERSION$ $BM.TXT_AUTHOR$: $AUTHOR$ $BM.TXT_DESC$: $DESC$" $ARG2$ "$ARG0$" "$ARG3$" "$ARG4$" "$ARG5$" "$TITLE$" "$PWD$" "$SAS$" "$SAS_NON_MC$"
    EXIT 0

:APP_CONFIG

    SETTITLE "$TITLE$ Config:"
    ADDWIDGET "LABEL" "Choose Compatibility Options:"
    ADDWIDGET "CHOICE" "SETBIOS Patches" "Patch BOOTROM for more compatibility" "BM.APPCNF_SYS_OSDMENU_BIOS" "OFF" "PS1" "PS2" "ENHANCED"
    ADDWIDGET "CHOICE" "Shutdown ModuleManager" "Shutdown memory pathches out of RAM." "BM.APPCNF_SYS_OSDMENU_MM" "NO" "MEMORY MODULE" "MMIOP" "ALL"
    ADDWIDGET "CHOICE" "Autoload File System Drivers for ELF" "Do not re-load Crystal Chip drivers for $TITLE$" "BM.APPCNF_SYS_OSDMENU_FSD" "NO" "YES"
    ADDWIDGET "LABEL" "Default: OFF, NO, NO"
    ADDWIDGET "CALL" "Delete $TITLE$ variables." "Deletes $TITLE$ variables from BootManager" "$ARG0$" "DEL_CFG"
    SAVEVARS "BM.APPCNF*" "$BM.BM_PATH$/CONFIG/APPCNF.PBT"
    EXIT 0

:DEL_CFG
    CLEARWIDGETS
    SKIPBACK
    UNSET "BM.APPCNF_SYS_OSDMENU_BIOS"
    UNSET "BM.APPCNF_SYS_OSDMENU_MM"
    UNSET "BM.APPCNF_SYS_OSDMENU_FSD"
    SAVEVARS "BM.APPCNF*" "$BM.BM_PATH$/CONFIG/APPCNF.PBT"
    GOTO "APP_CONFIG"
    EXIT 0

:INSTALL
        IF MATCHES "mc?" "$ARG2$"
            GOTO "INSTALL_TO_MC"
        ELSEIF NOT MATCHES "mc?" "$ARG2$"
            GOTO "INSTALL_TO_NONMC"
        ENDIF

:INSTALL_TO_MC
    IF FAIL COPY "$PWD$" "$ARG2$:/$SAS$"
        ECHO "Failed installing $TITLE$"
        MESSAGE "Failed installing $TITLE$"
        RRM "$ARG2$:/$SAS$"
        RETURN -1
    ENDIF
    EXIT 0

:INSTALL_TO_NONMC
    IF FAIL COPY "$PWD$" "$ARG2$:/$SAS_NON_MC$"
        ECHO "Failed installing $TITLE$"
        MESSAGE "Failed installing $TITLE$"
        RRM "$ARG2$:/$SAS_NON_MC$"
        RETURN -1
    ENDIF
    EXIT 0

:REMOVE
    IF FAIL RRM "$PWD$"
        IF FAIL RRM "$PWD$"
            IF FAIL RRM "$PWD$"
                MESSAGE "Failed removing $TITLE$!"
                RETURN -1
            ENDIF
        ENDIF
    ENDIF
    EXIT 0

:RUN
    IF MATCHES "$BM.APPCNF_SYS_OSDMENU_BIOS$$BM.APPCNF_SYS_OSDMENU_MM$$BM.APPCNF_SYS_OSDMENU_FSD$" ""
        SETBIOS "OFF"
        SET "BM.AUTOLOAD_FSD_EN" "0"
        GOTO "RUN_CONFIG"
    ENDIF

    # BIOS Patches
    IF EQU "$BM.APPCNF_SYS_OSDMENU_BIOS$" "0"
        SETBIOS "OFF"
    ELSEIF EQU "$BM.APPCNF_SYS_OSDMENU_BIOS$" "1"
        SETBIOS "PS1"
    ELSEIF EQU "$BM.APPCNF_SYS_OSDMENU_BIOS$" "2"
        SETBIOS "PS2"
    ENDIF
    #MEMORY MODULE Patches
    IF EQU "$BM.APPCNF_SYS_OSDMENU_MM$" "1"
        SHUTDOWN "MM"
    ELSEIF EQU "$BM.APPCNF_SYS_OSDMENU_MM$" "2"
        SHUTDOWN "MMIOP"
    ELSEIF EQU "$BM.APPCNF_SYS_OSDMENU_MM$" "3"
        SHUTDOWN "ALL"
    ENDIF
    # BM AUtoload file system drivers for next app
    IF EQU "$BM.APPCNF_SYS_OSDMENU_FSD$" "0"
        SET "BM.AUTOLOAD_FSD_EN" "0"
    ENDIF
    GOTO "RUN_CONFIG"

:RUN_CONFIG
    LOADEXEC "PBAT" "$BM.SCRIPTS$/LOADEXEC.PBT" "$PWD$/$ELF$"
    EXIT 0
