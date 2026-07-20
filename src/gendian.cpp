#include "gendian.h"
#include <stdint.h>

void fwrite8(FILE *file, unsigned data)
{
	uint8_t bytes[1];
	bytes[0] = (uint8_t)data;
	fwrite(bytes, 1, 1, file);
}

void fwritele16(FILE *file, unsigned data)
{
	uint8_t bytes[2];
	bytes[0] = (uint8_t)data;
	bytes[1] = (uint8_t)(data >> 8);
	fwrite(bytes, 2, 1, file);
}

void fwritele32(FILE *file, unsigned data)
{
	uint8_t bytes[4];
	bytes[0] = (uint8_t)data;
	bytes[1] = (uint8_t)(data >> 8);
	bytes[2] = (uint8_t)(data >> 16);
	bytes[3] = (uint8_t)(data >> 24);
	fwrite(bytes, 4, 1, file);
}

unsigned fread8(FILE *file)
{
	uint8_t bytes[1];
	fread(bytes, 1, 1, file);
	return bytes[0];
}

unsigned freadle16(FILE *file)
{
	uint8_t bytes[2];
	fread(bytes, 2, 1, file);
	return (unsigned)bytes[0] | ((unsigned)bytes[1] << 8);
}

unsigned freadle32(FILE *file)
{
	uint8_t bytes[4];
	fread(bytes, 4, 1, file);
	return (unsigned)bytes[0] | ((unsigned)bytes[1] << 8) |
	       ((unsigned)bytes[2] << 16) | ((unsigned)bytes[3] << 24);
}

unsigned freadhe16(FILE *file)
{
	uint8_t bytes[2];
	fread(bytes, 2, 1, file);
	return (unsigned)bytes[1] | ((unsigned)bytes[0] << 8);
}

unsigned freadhe32(FILE *file)
{
	uint8_t bytes[4];
	fread(bytes, 4, 1, file);
	return (unsigned)bytes[3] | ((unsigned)bytes[2] << 8) |
	       ((unsigned)bytes[1] << 16) | ((unsigned)bytes[0] << 24);
}
