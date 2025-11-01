#ifndef _HDD_H_
#define _HDD_H_

// Functions implemented in hdd.c

// Puts HDD in idle mode and powers off the dev9 device
void shutdownDEV9();

// Runs HDD checks and returns:
// - `-1` on error
// - `0` on success
// - `1` if any of the PFS partitions need checking
int isFsckRequired();

// Finds and launches the fsck utility
void runFsck();

// Mounts the partition specified in path
int mountPFS(char *path);

// Unmounts the currently mounted partition
int umountPFS();

// Functions implemented in hdd_start.c

// Starts HDD application using data from the partition attribute area header
int handleHDDApplication(int argc, char *argv[]);

#endif
