#ifndef _COMMON_H_
#define _COMMON_H_

// Returns >=0 if file exists
int checkFile(char *path);

// Displays the message on the screen
void msg(const char *str, ...);

// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail(char *msg);

#endif
