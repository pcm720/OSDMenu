#ifndef _HANDLERS_H_
#define _HANDLERS_H_

// Handles arguments supported by the HDD-OSD/PSBBN MBR
int handleOSDArgs(int argc, char *argv[]);

// Starts the executable pointed to by argv[0]
int handleConfigPath(int argc, char *argv[]);

// Starts the dnasload applcation
void handleDNAS(int argc, char *argv[]);

// Starts HDD application using data from the partition attribute area header
// Assumes argv[0] is the partition path
int handleHDDApplication(int argc, char *argv[]);

#endif
