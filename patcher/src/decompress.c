#include <elf.h>
#include <kernel.h>
#include <stdint.h>
#include <string.h>

// Based on code by balika011
// https://gist.github.com/balika011/7a2443011c3a79ea53e0b98edb905a86

// Finds the start of the data segment in the packed OSDSYS binary
uint8_t *findDataSegment(uint8_t *buf) {
  Elf32_Ehdr *ehdr = (Elf32_Ehdr *)buf;

  Elf32_Shdr *shdr = (Elf32_Shdr *)&buf[ehdr->e_shoff];

  uint8_t *strtab = 0;

  for (int i = 0; i < ehdr->e_shnum; i++) {
    if (shdr[i].sh_type == SHT_STRTAB) {
      strtab = &buf[shdr[i].sh_offset];
      break;
    }
  }

  if (strtab) {
    for (int i = 0; i < ehdr->e_shnum; i++) {
      if (strcmp(".data", (char *)&strtab[shdr[i].sh_name]) == 0)
        return &buf[shdr[i].sh_offset];
    }
  }

  return NULL;
}

// Decompresses data segment to dst
void lzDecompress(uint8_t *dest, uint8_t *source, uint32_t dest_size) {
  uint8_t *ptr = dest;

  uint32_t flag = 0, count = 0, mask = 0, shift = 0;
  while (ptr - dest < dest_size) {
    if (count == 0) {
      count = 30;
      flag = ((uint32_t)source[0] << 24) | ((uint32_t)source[1] << 16) | ((uint32_t)source[2] << 8) | (uint32_t)source[3];
      source += 4;
      mask = 0x3fff >> (flag & 3);
      shift = 14 - (flag & 3);
    }

    if (flag & 0x80000000) {
      uint16_t off_size = ((uint16_t)source[0] << 8) | (uint16_t)source[1];
      source += 2;
      uint16_t offset = (off_size & mask) + 1;
      for (uint16_t i = (off_size >> shift) + 3; i > 0; i--) {
        *ptr = *(ptr - offset);
        ptr++;
      }
    } else
      *(ptr++) = *(source++);

    count--;
    flag <<= 1;
  }
}

int decompressOSDSYS(uint8_t *dst, uint8_t *src, uint32_t src_size) {
  uint8_t *data = findDataSegment(src);
  if (!data)
    return -1;

  lzDecompress(dst, &data[4], *(uint32_t *)data);
  FlushCache(0);
  FlushCache(2);
  return 0;
}
