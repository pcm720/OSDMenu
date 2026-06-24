#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#include <stdint.h>

// Decompresses OSDSYS from src buffer into dst
int decompressOSDSYS(uint8_t *dst, uint8_t *src, uint32_t src_size);

#endif
