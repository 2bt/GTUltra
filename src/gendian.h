#pragma once

#include <cstdio>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void     fwrite8(FILE* file, unsigned data);
void     fwritele16(FILE* file, unsigned data);
void     fwritele32(FILE* file, unsigned data);
unsigned fread8(FILE* file);
unsigned freadle32(FILE* file);

#ifdef __cplusplus
}
#endif
