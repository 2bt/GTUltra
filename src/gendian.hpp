#pragma once

#include <cstdio>

// Little-endian file I/O helpers. Values are read/written LSB first,
// regardless of host byte order.

void     fwrite8(FILE* file, unsigned data);
void     fwritele16(FILE* file, unsigned data);
void     fwritele32(FILE* file, unsigned data);
unsigned fread8(FILE* file);
unsigned freadle32(FILE* file);
