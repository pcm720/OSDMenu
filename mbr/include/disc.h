#ifndef _DISC_H_
#define _DISC_H_

// Starts PS1/PS2 game CD/DVD
int startGameDisc();

// Boots PS2 disc directly or via PS2LOGO
void handlePS2Disc(char *bootPath, char *eGSMArgument);

// Starts the DVD Player from a memory card or ROM
int startDVDVideo();

// Executes the sceCdAutoAdjustCtrl call
void cdAutoAdjust(int mode);

// Updates MechaCon NVRAM config with selected PS1DRV options, screen type and language.
void updateOSDSettings();

#endif
