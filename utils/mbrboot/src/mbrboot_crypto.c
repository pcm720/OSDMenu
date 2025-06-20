// PSBBN MBR rom0:MBRBOOT argument decryption code
// Based on decompiled code regurgitated by LLM and uyjulian HDD OSD reverese engineering efforts
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Performs a bit transposition operation on a 64-bit input value.
uint64_t transposeBytes(uint64_t value);
// Performs a complex bit transposition operation on a 64-bit input value.
uint64_t transposeNonLinearBits(uint64_t input);
// Remaps bits from a 32-bit input value to new positions in a 64-bit output using a predefined lookup table.
uint64_t remapBits(uint64_t input);
// Applies S-box substitutions to 6-bit chunks of the input value.
uint64_t applySBoxSubstitution(uint64_t input);
// Remaps bits from a 32-bit input value to new positions in a 64-bit output using a predefined lookup table.
uint64_t permuteBits(uint64_t input);
// Derives 16 DES subkeys from a given 64-bit key
void generateSubkeys(uint64_t *output_array, uint64_t key);
// Deobfuscates base64-encoded argument string in-place using ROT13
int deobfuscateString(char *input);
// Converts a Base64 encoded string to its original binary form
int base64Decode(char *output, const char *input, int inputSize);

// Decrypts PSBBN MBRBOOT arguments
char **decryptMBRBOOTArgs(int *argc, char **argv) {
  uint64_t subkeys[32] = {0};         // Decryption keys
  uint64_t decryptedData[32] = {0};   // Buffer for decrypted data
  uint64_t magic = 0xdb583a35432cebd; // Set initial value

  // Parse first argument as the argument count
  int argCount;
  if (sscanf(argv[1], "%d", &argCount) != 1) {
    *argc = 1;
    return argv;
  }

  // Parse second argument as the pointer to argument array
  char **outputArgs;
  if (sscanf(argv[2], "%p", &outputArgs) != 1) {
    *argc = 1;
    return argv;
  }

  // Decode the first argument to get DES keys
  int stringLength = strlen(*outputArgs);
  int decodedLength = base64Decode((char *)decryptedData, *outputArgs, stringLength);

  if (decodedLength < 0) {
    // Decoding failed
    *argc = 1;
    printf("Decoding failed\n");
    return argv;
  }

  // Generate decryption subkeys from DES keys
  generateSubkeys(subkeys, decryptedData[0]);      // subkeys 0-15
  generateSubkeys(&subkeys[16], decryptedData[1]); // subkeys 16-31

  // Set up the new argc/argv values
  *argc = argCount;
  *outputArgs = *argv;

  if (argCount <= 1) {
    return outputArgs;
  }

  // For each argument
  for (int i = 1; i < argCount; i++) {
    // Clear buffer for decoding
    memset(decryptedData, 0, sizeof(decryptedData));

    // Get current argument
    char *currentArg = outputArgs[i];

    // Deobfuscate the string
    deobfuscateString(currentArg);

    // Base64 decode the argument
    stringLength = strlen(currentArg);
    decodedLength = base64Decode((char *)decryptedData, currentArg, stringLength);

    if (decodedLength < 0) {
      // Invalid argument
      outputArgs[i] = strdup("BootError");
      continue;
    }

    // Calculate the number of 8-byte blocks to process
    int blockCount = (decodedLength + 7) / 8;

    // Process each 8-byte block
    for (int j = 0; j < blockCount; j++) {
      // Initialize variables
      uint64_t tempData = decryptedData[j];
      uint64_t substitutionResult = 0;
      uint64_t tempValue = tempData;
      uint64_t upperBits;
      uint64_t lowerBits;
      int keyIdx = 0;

      // Do three decryption stages
      // 1. 16 rounds with subkeys 0-15
      // 2. 16 rounds with subkeys 16-31
      // 3. 16 rounds with subkeys 0-15
      for (int stage = 0; stage < 3; stage++) {
        upperBits = transposeBytes(tempValue);
        lowerBits = upperBits << 32;
        upperBits &= 0xffffffff00000000;

        for (int rounds = 15; rounds >= 0; rounds--) {
          // Use subkeys 15-0 for stages 1 and 3, subkeys 16-31 for stage 2
          keyIdx = (stage == 1) ? ((15 - rounds) + 16) : rounds;

          tempValue = lowerBits;
          lowerBits = remapBits(tempValue);
          substitutionResult = applySBoxSubstitution((lowerBits ^ subkeys[keyIdx]) >> 16);
          lowerBits = permuteBits(substitutionResult);
          lowerBits ^= upperBits;
          upperBits = tempValue;
        }
        // Prepare data for the next stage / do the final transformation
        tempValue = transposeNonLinearBits(lowerBits | (tempValue >> 32));
      }

      // XOR with magic
      decryptedData[j] = tempValue ^ magic;
      magic = tempData;
    }

    // Copy decrypted data back to argument
    strcpy(currentArg, (char *)decryptedData);
  }

  return outputArgs;
}

// Performs a bit transposition operation on a 64-bit input value.
uint64_t transposeBytes(uint64_t value) {
  uint64_t result = 0;

  for (int i = 7; i >= 0; i--) {
    uint64_t bit_mask = 1ULL << (i & 0x1f);
    uint8_t byte = value & 0xFF;

    // Process each bit of the current byte and place it in the transposed position
    if (byte & 0x01)
      result |= bit_mask << 0x20;
    if (byte & 0x02)
      result |= bit_mask;
    if (byte & 0x04)
      result |= bit_mask << 0x28;
    if (byte & 0x08)
      result |= bit_mask << 8;
    if (byte & 0x10)
      result |= bit_mask << 0x30;
    if (byte & 0x20)
      result |= bit_mask << 0x10;
    if (byte & 0x40)
      result |= bit_mask << 0x38;
    if (byte & 0x80)
      result |= bit_mask << 0x18;

    // Move to next byte
    value >>= 8;
  }

  return result;
}

// Performs a complex bit transposition operation on a 64-bit input value.
uint64_t transposeNonLinearBits(uint64_t input) {
  uint64_t result = 0;

  // Process one byte at a time
  for (uint32_t byteIndex = 0; byteIndex < 8; byteIndex++) {
    // Calculate the bit position
    uint32_t bitPosition = ((byteIndex >> 2) ^ 1) | ((byteIndex & 3) << 1);
    uint64_t bitValue = 1ULL << bitPosition;

    // Extract the current byte
    uint8_t currentByte = (input >> (byteIndex * 8)) & 0xFF;

    // Map each bit from the current byte to its new position in reverse order
    if (currentByte & 0x01)
      result |= (bitValue << 0x38); // Bit 0 -> Position 56+
    if (currentByte & 0x02)
      result |= (bitValue << 0x30); // Bit 1 -> Position 48+
    if (currentByte & 0x04)
      result |= (bitValue << 0x28); // Bit 2 -> Position 40+
    if (currentByte & 0x08)
      result |= (bitValue << 0x20); // Bit 3 -> Position 32+
    if (currentByte & 0x10)
      result |= (bitValue << 0x18); // Bit 4 -> Position 24+
    if (currentByte & 0x20)
      result |= (bitValue << 0x10); // Bit 5 -> Position 16+
    if (currentByte & 0x40)
      result |= (bitValue << 0x08); // Bit 6 -> Position 8+
    if (currentByte & 0x80)
      result |= (bitValue); // Bit 7 -> Position 0+
  }

  return result;
}

// Remaps bits from a 32-bit input value to new positions in a 64-bit output using a predefined lookup table.
uint64_t remapBits(uint64_t input) {
  // Lookup table defining where each input bit maps to in the output
  const uint64_t BIT_MAPPING_TABLE[32] = {
      0x8000000000020000, 0x0000000000040000, 0x0000000000080000, 0x0000000000500000, 0x0000000000a00000, 0x0000000001000000, 0x0000000002000000,
      0x0000000014000000, 0x0000000028000000, 0x0000000040000000, 0x0000000080000000, 0x0000000500000000, 0x0000000a00000000, 0x0000001000000000,
      0x0000002000000000, 0x0000014000000000, 0x0000028000000000, 0x0000040000000000, 0x0000080000000000, 0x0000500000000000, 0x0000a00000000000,
      0x0001000000000000, 0x0002000000000000, 0x0014000000000000, 0x0028000000000000, 0x0040000000000000, 0x0080000000000000, 0x0500000000000000,
      0x0a00000000000000, 0x1000000000000000, 0x2000000000000000, 0x4000000000010000};

  uint64_t result = 0;

  // Process all 32 bits from position 32 to 63
  for (int i = 0; i < 32; i++) {
    // Check if bit at position (32+i) is set
    if ((input >> (32 + i)) & 1) {
      // If set, OR the corresponding table value into the result
      result |= BIT_MAPPING_TABLE[i];
    }
  }
  return result;
}

// Applies S-box substitutions to 6-bit chunks of the input value.
uint64_t applySBoxSubstitution(uint64_t input) {
  // S-box lookup table - contains 8 S-boxes, each with 64 entries (4-bit values)
  const uint8_t S_BOXES[512] = {
      0x0D, 0x02, 0x08, 0x04, 0x06, 0x0F, 0x0B, 0x01, 0x0A, 0x09, 0x03, 0x0E, 0x05, 0x00, 0x0C, 0x07, 0x01, 0x0F, 0x0D, 0x08, 0x0A, 0x03, 0x07, 0x04,
      0x0C, 0x05, 0x06, 0x0B, 0x00, 0x0E, 0x09, 0x02, 0x07, 0x0B, 0x04, 0x01, 0x09, 0x0C, 0x0E, 0x02, 0x00, 0x06, 0x0A, 0x0D, 0x0F, 0x03, 0x05, 0x08,
      0x02, 0x01, 0x0E, 0x07, 0x04, 0x0A, 0x08, 0x0D, 0x0F, 0x0C, 0x09, 0x00, 0x03, 0x05, 0x06, 0x0B, 0x04, 0x0B, 0x02, 0x0E, 0x0F, 0x00, 0x08, 0x0D,
      0x03, 0x0C, 0x09, 0x07, 0x05, 0x0A, 0x06, 0x01, 0x0D, 0x00, 0x0B, 0x07, 0x04, 0x09, 0x01, 0x0A, 0x0E, 0x03, 0x05, 0x0C, 0x02, 0x0F, 0x08, 0x06,
      0x01, 0x04, 0x0B, 0x0D, 0x0C, 0x03, 0x07, 0x0E, 0x0A, 0x0F, 0x06, 0x08, 0x00, 0x05, 0x09, 0x02, 0x06, 0x0B, 0x0D, 0x08, 0x01, 0x04, 0x0A, 0x07,
      0x09, 0x05, 0x00, 0x0F, 0x0E, 0x02, 0x03, 0x0C, 0x0C, 0x01, 0x0A, 0x0F, 0x09, 0x02, 0x06, 0x08, 0x00, 0x0D, 0x03, 0x04, 0x0E, 0x07, 0x05, 0x0B,
      0x0A, 0x0F, 0x04, 0x02, 0x07, 0x0C, 0x09, 0x05, 0x06, 0x01, 0x0D, 0x0E, 0x00, 0x0B, 0x03, 0x08, 0x09, 0x0E, 0x0F, 0x05, 0x02, 0x08, 0x0C, 0x03,
      0x07, 0x00, 0x04, 0x0A, 0x01, 0x0D, 0x0B, 0x06, 0x04, 0x03, 0x02, 0x0C, 0x09, 0x05, 0x0F, 0x0A, 0x0B, 0x0E, 0x01, 0x07, 0x06, 0x00, 0x08, 0x0D,
      0x02, 0x0C, 0x04, 0x01, 0x07, 0x0A, 0x0B, 0x06, 0x08, 0x05, 0x03, 0x0F, 0x0D, 0x00, 0x0E, 0x09, 0x0E, 0x0B, 0x02, 0x0C, 0x04, 0x07, 0x0D, 0x01,
      0x05, 0x00, 0x0F, 0x0A, 0x03, 0x09, 0x08, 0x06, 0x04, 0x02, 0x01, 0x0B, 0x0A, 0x0D, 0x07, 0x08, 0x0F, 0x09, 0x0C, 0x05, 0x06, 0x03, 0x00, 0x0E,
      0x0B, 0x08, 0x0C, 0x07, 0x01, 0x0E, 0x02, 0x0D, 0x06, 0x0F, 0x00, 0x09, 0x0A, 0x04, 0x05, 0x03, 0x07, 0x0D, 0x0E, 0x03, 0x00, 0x06, 0x09, 0x0A,
      0x01, 0x02, 0x08, 0x05, 0x0B, 0x0C, 0x04, 0x0F, 0x0D, 0x08, 0x0B, 0x05, 0x06, 0x0F, 0x00, 0x03, 0x04, 0x07, 0x02, 0x0C, 0x01, 0x0A, 0x0E, 0x09,
      0x0A, 0x06, 0x09, 0x00, 0x0C, 0x0B, 0x07, 0x0D, 0x0F, 0x01, 0x03, 0x0E, 0x05, 0x02, 0x08, 0x04, 0x03, 0x0F, 0x00, 0x06, 0x0A, 0x01, 0x0D, 0x08,
      0x09, 0x04, 0x05, 0x0B, 0x0C, 0x07, 0x02, 0x0E, 0x0A, 0x00, 0x09, 0x0E, 0x06, 0x03, 0x0F, 0x05, 0x01, 0x0D, 0x0C, 0x07, 0x0B, 0x04, 0x02, 0x08,
      0x0D, 0x07, 0x00, 0x09, 0x03, 0x04, 0x06, 0x0A, 0x02, 0x08, 0x05, 0x0E, 0x0C, 0x0B, 0x0F, 0x01, 0x0D, 0x06, 0x04, 0x09, 0x08, 0x0F, 0x03, 0x00,
      0x0B, 0x01, 0x02, 0x0C, 0x05, 0x0A, 0x0E, 0x07, 0x01, 0x0A, 0x0D, 0x00, 0x06, 0x09, 0x08, 0x07, 0x04, 0x0F, 0x0E, 0x03, 0x0B, 0x05, 0x02, 0x0C,
      0x0F, 0x01, 0x08, 0x0E, 0x06, 0x0B, 0x03, 0x04, 0x09, 0x07, 0x02, 0x0D, 0x0C, 0x00, 0x05, 0x0A, 0x03, 0x0D, 0x04, 0x07, 0x0F, 0x02, 0x08, 0x0E,
      0x0C, 0x00, 0x01, 0x0A, 0x06, 0x09, 0x0B, 0x05, 0x00, 0x0E, 0x07, 0x0B, 0x0A, 0x04, 0x0D, 0x01, 0x05, 0x08, 0x0C, 0x06, 0x09, 0x03, 0x02, 0x0F,
      0x0D, 0x08, 0x0A, 0x01, 0x03, 0x0F, 0x04, 0x02, 0x0B, 0x06, 0x07, 0x0C, 0x00, 0x05, 0x0E, 0x09, 0x0E, 0x04, 0x0D, 0x01, 0x02, 0x0F, 0x0B, 0x08,
      0x03, 0x0A, 0x06, 0x0C, 0x05, 0x09, 0x00, 0x07, 0x00, 0x0F, 0x07, 0x04, 0x0E, 0x02, 0x0D, 0x01, 0x0A, 0x06, 0x0C, 0x0B, 0x09, 0x05, 0x03, 0x08,
      0x04, 0x01, 0x0E, 0x08, 0x0D, 0x06, 0x02, 0x0B, 0x0F, 0x0C, 0x09, 0x07, 0x03, 0x0A, 0x05, 0x00, 0x0F, 0x0C, 0x08, 0x02, 0x04, 0x09, 0x01, 0x07,
      0x05, 0x0B, 0x03, 0x0E, 0x0A, 0x00, 0x06, 0x0D};

  uint64_t result = 0;

  // Process 8 chunks of 6 bits each
  for (int i = 0; i < 8; i++) {
    // Extract the current 6-bit chunk
    uint64_t sixBitChunk = (input >> (6 * i)) & 0x3F;

    // Calculate the S-box index
    uint8_t sBoxIndex = (sixBitChunk & 0x20) |       // bit 5
                        ((sixBitChunk >> 1) & 0xF) | // bits 1-4
                        ((sixBitChunk & 1) << 4);    // bit 0

    // Look up the 4-bit result in the appropriate S-box (offset by 64*i)
    // and place it in the appropriate position in the result
    result |= ((uint64_t)S_BOXES[0x40 * i + sBoxIndex]) << (i * 4);
  }

  return result;
}

// Remaps bits from a 32-bit input value to new positions in a 64-bit output using a predefined lookup table.
uint64_t permuteBits(uint64_t input) {
  // Lookup table defining where each input bit maps to in the output
  const uint32_t BIT_MAPPING_TABLE[32] = {0x00000800, 0x00020000, 0x00000020, 0x08000000, 0x02000000, 0x00000400, 0x00100000, 0x00000001,
                                          0x00002000, 0x00200000, 0x00000008, 0x10000000, 0x20000000, 0x00000080, 0x00040000, 0x01000000,
                                          0x80000000, 0x00400000, 0x00001000, 0x00000040, 0x04000000, 0x00000004, 0x00010000, 0x00000100,
                                          0x00004000, 0x40000000, 0x00000010, 0x00080000, 0x00000002, 0x00000200, 0x00008000, 0x00800000};

  uint32_t result = 0;

  // Check each bit position in the input
  for (int bitPosition = 0; bitPosition < 32; bitPosition++) {
    // If the bit is set in the input, OR the corresponding value from the table
    if ((input >> bitPosition) & 1) {
      result |= BIT_MAPPING_TABLE[bitPosition];
    }
  }

  return (uint64_t)result << 32;
}

// Derives 16 DES subkeys from a given 64-bit key
void generateSubkeys(uint64_t *output_array, uint64_t key) {
  // Bit position lookup table 1 (56 entries)
  const uint8_t BIT_POSITIONS_TABLE_1[0x38] = {0x07, 0x0F, 0x17, 0x1F, 0x27, 0x2F, 0x37, 0x3F, 0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E,
                                               0x36, 0x3E, 0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D, 0x04, 0x0C, 0x14, 0x1C,
                                               0x01, 0x09, 0x11, 0x19, 0x21, 0x29, 0x31, 0x39, 0x02, 0x0A, 0x12, 0x1A, 0x22, 0x2A,
                                               0x32, 0x3A, 0x03, 0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B, 0x24, 0x2C, 0x34, 0x3C};

  // Bit position lookup table 2 (48 entries)
  const uint8_t BIT_POSITIONS_TABLE_2[0x30] = {0x32, 0x2F, 0x35, 0x28, 0x3F, 0x3B, 0x3D, 0x24, 0x31, 0x3A, 0x2B, 0x36, 0x29, 0x2D, 0x34, 0x3C,
                                               0x26, 0x38, 0x30, 0x39, 0x25, 0x2C, 0x33, 0x3E, 0x17, 0x0C, 0x21, 0x1B, 0x11, 0x09, 0x22, 0x18,
                                               0x0D, 0x13, 0x1F, 0x10, 0x14, 0x0F, 0x19, 0x08, 0x1E, 0x0B, 0x12, 0x16, 0x0E, 0x1C, 0x23, 0x20};

  // Perform initial bit permutation using BIT_POSITIONS_TABLE_1
  uint64_t permuted_value = 0;
  uint64_t bit_mask = 0x8000000000000000; // Most significant bit

  for (int i = 0; i < 0x38; i++) {
    if (key & (1ULL << BIT_POSITIONS_TABLE_1[i])) {
      permuted_value |= bit_mask;
    }
    bit_mask >>= 1;
  }

  // Split the permuted value into two parts
  uint64_t high_bits = permuted_value >> 0x24;           // Top 8 bits
  uint64_t low_bits = (permuted_value >> 8) & 0xFFFFFFF; // 28 bits

  // Generate 16 output values
  for (int round = 0x10; round > 0; round--) {
    uint32_t high_32 = (uint32_t)high_bits;
    uint32_t low_32 = (uint32_t)low_bits;
    uint32_t high_shift, low_shift;

    // Perform different shifts based on the round number
    if ((round < 0xF && round != 8) && round != 1) {
      // Shift by 2 bits for most rounds
      high_shift = high_32 >> 0x1A; // Top 6 bits
      low_shift = low_32 >> 0x1A;   // Top 6 bits
      high_32 = high_32 << 2;       // Shift left 2
      low_32 = low_32 << 2;         // Shift left 2
    } else {
      // Shift by 1 bit for special rounds (1, 8, 15, 16)
      high_shift = high_32 >> 0x1B; // Top 5 bits
      low_shift = low_32 >> 0x1B;   // Top 5 bits
      high_32 = high_32 << 1;       // Shift left 1
      low_32 = low_32 << 1;         // Shift left 1
    }

    // Recombine the shifted values
    high_bits = ((uint64_t)high_32 & 0xFFFFFFF) | high_shift;
    low_bits = ((uint64_t)low_32 | low_shift) & 0xFFFFFFF;

    // Perform second bit permutation using BIT_POSITIONS_TABLE_2
    uint64_t combined_value = (high_bits << 0x24) | (low_bits << 8);
    uint64_t output_value = 0;
    bit_mask = 0x8000000000000000; // Reset bit mask

    for (int j = 0; j < 0x30; j++) {
      if (combined_value & (1ULL << BIT_POSITIONS_TABLE_2[j])) {
        output_value |= bit_mask;
      }
      bit_mask >>= 1;
    }

    // Store the result in the output array
    *output_array = output_value;
    output_array++;
  }
}

// Deobfuscates base64-encoded argument string in-place using ROT13
int deobfuscateString(char *input) {
  int length = 0;
  // Process each character until we hit a null terminator
  do {
    // Rotate only alphabet symbols
    if ((*input >= 'a' && *input <= 'z') || (*input >= 'A' && *input <= 'Z')) {
      if ((*input > 'm') || (*input > 'M' && *input <= 'Z'))
        *input -= 13;
      else
        *input += 13;
    }

    input++;
    length++;
  } while (*input != '\0');

  // Return the length of the transformed string (excluding null terminator)
  return length - 1; // Subtract 1 because we don't count the null terminator
}

// Converts a Base64 encoded string to its original binary form
int base64Decode(char *output, const char *input, int inputSize) {
  // Lookup table for Base64 character values
  static const uint8_t base64DecodingTable[] = {
      /* ASCII table positions 0-63 */
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF,
      0x3F,                                                                                           // '+' -> 62, '/' -> 63
      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, // '0'-'9' -> 52-61
      0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, // 'A'-'O' -> 0-14
      0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 'P'-'Z' -> 15-25
      0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // 'a'-'o' -> 26-40
      0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 'p'-'z' -> 41-51
      /* Remainder of ASCII table - all invalid */
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  int outputSize = 0;

  // Process input in 4-byte chunks (Base64 groups)
  while (inputSize > 0) {
    // Check for padding ('=') characters and adjust output size
    if (input[2] == '=') // '=' is ASCII 0x3d
      outputSize--;
    if (input[3] == '=')
      outputSize--;

    // Validate all 4 characters in the group
    char value0 = base64DecodingTable[(uint8_t)input[0]];
    char value1 = base64DecodingTable[(uint8_t)input[1]];
    char value2 = base64DecodingTable[(uint8_t)input[2]];
    char value3 = base64DecodingTable[(uint8_t)input[3]];

    if (value0 < 0 || value1 < 0 || value2 < 0 || value3 < 0)
      // Invalid Base64 character
      return -1;

    uint32_t combinedValue = (value0 << 18) | (value1 << 12) | (value2 << 6) | value3;

    // Extract the 3 bytes
    output[0] = (char)(combinedValue >> 16);
    output[1] = (char)(combinedValue >> 8);
    output[2] = (char)combinedValue;

    // Move pointers and update counters
    input += 4;
    output += 3;
    inputSize -= 4;
    outputSize += 3;
  }

  // Null-terminate the output string
  *output = '\0';

  return outputSize;
}
