#ifndef _MBRBOOT_CRYPTO_H_
#define _MBRBOOT_CRYPTO_H_

// Decrypts PSBBN MBRBOOT arguments
char **decryptMBRBOOTArgs(int *argc, char **argv);

// Encrypts arguments for PSBBN osdboot.elf
char **encryptOSDBOOTArgs(int *argc, char **argv);

#endif
