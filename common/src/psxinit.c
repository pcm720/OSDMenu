#include <iopcontrol.h>
#include <kernel.h>
#include <loadfile.h>
#include <osd_config.h>
#include <sifrpc-common.h>
#include <sifrpc.h>
#include <tamtypes.h>

// PSX-only SCMDs
#define CD_SCMD_CHG_SYS 0x2D
#define CD_SCMD_NOTICE_GAME_START 0x2F

// libcdvd externs
extern int bindSCmd;
extern SifRpcClientData_t clientSCmd;
extern int sCmdSemaId;
extern u8 sCmdRecvBuff[];
extern u8 sCmdSendBuff[];
extern int sCmdNum;
extern int _CdCheckSCmd(int cmd);

//
// SCMD functions not exposed in PS2SDK EE libcdvd
//

// Switches PSX between Rainbow and PS2 modes
int custom_sceCdChgSys(int mode) {
  int result;

  if (_CdCheckSCmd(CD_SCMD_CHG_SYS) == 0)
    return 0;

  *(int *)sCmdSendBuff = mode;
  if (SifCallRpc(&clientSCmd, CD_SCMD_CHG_SYS, 0, sCmdSendBuff, 4, sCmdRecvBuff, 4, NULL, NULL) >= 0) {
    result = *(int *)UNCACHED_SEG(sCmdRecvBuff);
  } else {
    result = 0;
  }

  SignalSema(sCmdSemaId);
  return result;
}

// Enables "Quit Game" button
int custom_sceCdNoticeGameStart(int mode, u32 *result) {
  int status;

  if (_CdCheckSCmd(CD_SCMD_NOTICE_GAME_START) == 0)
    return 0;

  *(u32 *)sCmdSendBuff = mode;
  if (SifCallRpc(&clientSCmd, CD_SCMD_NOTICE_GAME_START, 0, sCmdSendBuff, 4, sCmdRecvBuff, 8, NULL, NULL) >= 0) {
    *result = *(u32 *)UNCACHED_SEG(&sCmdRecvBuff[4]);
    status = *(int *)UNCACHED_SEG(sCmdRecvBuff);
  } else {
    status = 0;
  }

  SignalSema(sCmdSemaId);
  return status;
}

// Reboots IOP with OSDSYS modules and switches PSX into PS2 mode.
void switchPSX() {
  // Reboot IOP with PSX OSDSYS modules to switch PSX into PS2 mode
  sceSifInitRpc(0);
  while (!SifIopReset("rom0:UDNL rom0:OSDCNF", 0))
    ;
  while (!SifIopSync())
    ;
  sceSifInitRpc(0);

  // Execute SCMDs
  sceCdInit(SCECdINoD);
  // Switch the drive into PS2 mode.
  while (custom_sceCdChgSys(2) != 2) {
  };

  // Enable Exit button
  u32 stat;
  int result;
  do {
    result = custom_sceCdNoticeGameStart(1, &stat);
  } while ((result == 0) || (stat & 0x80));
  sceCdInit(SCECdEXIT);
}
