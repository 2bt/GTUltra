#include "gendian.h"

void fwrite8(FILE* file, unsigned data) {
    uint8_t bytes[1];
    bytes[0] = static_cast<uint8_t>(data);
    fwrite(bytes, 1, 1, file);
}

void fwritele16(FILE* file, unsigned data) {
    uint8_t bytes[2];
    bytes[0] = static_cast<uint8_t>(data);
    bytes[1] = static_cast<uint8_t>(data >> 8);
    fwrite(bytes, 2, 1, file);
}

void fwritele32(FILE* file, unsigned data) {
    uint8_t bytes[4];
    bytes[0] = static_cast<uint8_t>(data);
    bytes[1] = static_cast<uint8_t>(data >> 8);
    bytes[2] = static_cast<uint8_t>(data >> 16);
    bytes[3] = static_cast<uint8_t>(data >> 24);
    fwrite(bytes, 4, 1, file);
}

unsigned fread8(FILE* file) {
    uint8_t bytes[1];
    if (fread(bytes, 1, 1, file) != 1) return 0;
    return bytes[0];
}

unsigned freadle32(FILE* file) {
    uint8_t bytes[4];
    if (fread(bytes, 4, 1, file) != 1) return 0;
    return static_cast<unsigned>(bytes[0]) | (static_cast<unsigned>(bytes[1]) << 8) |
           (static_cast<unsigned>(bytes[2]) << 16) | (static_cast<unsigned>(bytes[3]) << 24);
}
