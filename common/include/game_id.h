#ifndef _GAME_ID_H_
#define _GAME_ID_H_

// Initializes GS and displays visual game ID
void gsDisplayGameID(const char *gameID);

// Attempts to generate a title ID from path
void generateTitleIDFromELF(char *path, char *dst);

#endif
