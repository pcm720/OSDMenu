#ifndef _GAME_ID_H_
#define _GAME_ID_H_

// Initializes GS and displays visual game ID
void gsDisplayGameID(const char *gameID);

// Updates the history file and shows game ID
void updateLaunchHistory(char *titleID, int showAppID);

// Returns 1 if ID is a valid PS2 title ID
int validateTitleID(char *titleID);

#endif
