#ifndef _COMMON_H_
#define _COMMON_H_

// Returns >=0 if file exists
int checkFile(char *path);

// Checks both memory cards for a file in path
// If the path starts with mc?, replaces the '? with the memory card number
int checkMCPath(char *path);

// Checks whether the file exists
int checkPFSPath(char *path);

// Displays the message on the screen
void msg(const char *str, ...);

// Displays the fatal error message on the screen
void fatalMsg(char *str);

// Starts HOSDMenu or HDD-OSD.
// Assumes the system partition is already mounted and argv has space for the additional -mbrboot argument.
// Will unmount the partition.
int startHOSDSYS(int argc, char *argv[]);

// Attempts to launch PSBBN, HOSDMenu or HOSDSYS. Falls back to OSDSYS
int execOSD(int argc, char *argv[]);

// Shuts down HDD, dev9 and exits to OSD with arguments
void bootFailWithArgs(char *msg, int argc, char *argv[]);

// Shuts down HDD, dev9 and exits to OSDSYS
void bootFail(char *msg);

#endif
