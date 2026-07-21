#include "gendian.hpp"

#include <cstdint>

void fwrite8(FILE* file, unsigned data) {
    auto byte = static_cast<uint8_t>(data);
    fwrite(&byte, 1, 1, file);
}

void fwritele16(FILE* file, unsigned data) {
    const uint8_t bytes[2] = {
        static_cast<uint8_t>(data),
        static_cast<uint8_t>(data >> 8),
    };
    fwrite(bytes, sizeof bytes, 1, file);
}

void fwritele32(FILE* file, unsigned data) {
    const uint8_t bytes[4] = {
        static_cast<uint8_t>(data),
        static_cast<uint8_t>(data >> 8),
        static_cast<uint8_t>(data >> 16),
        static_cast<uint8_t>(data >> 24),
    };
    fwrite(bytes, sizeof bytes, 1, file);
}

unsigned fread8(FILE* file) {
    uint8_t byte;
    if (fread(&byte, 1, 1, file) != 1) return 0;
    return byte;
}

unsigned freadle32(FILE* file) {
    uint8_t bytes[4];
    if (fread(bytes, sizeof bytes, 1, file) != 1) return 0;
    return static_cast<unsigned>(bytes[0]) | (static_cast<unsigned>(bytes[1]) << 8) |
           (static_cast<unsigned>(bytes[2]) << 16) | (static_cast<unsigned>(bytes[3]) << 24);
}
