#include "gplatform.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int bme_error = BME_OK;

typedef struct
{
	Uint32 offset;
	Sint32 length;
	char name[13];
} HEADER;

typedef struct
{
	HEADER *currentheader;
	int filepos;
	int open;
} HANDLE;

static int io_usedatafile = 0;
static HEADER *fileheaders = nullptr;
static unsigned files = 0;
static char ident[4];
static const char *idstring = "DAT!";
static HANDLE handle[MAX_HANDLES];
static FILE *fileptr[MAX_HANDLES] = { nullptr };
static FILE *datafilehandle = nullptr;
static unsigned char *datafileptr = nullptr;
static unsigned char *datafilestart = nullptr;

static unsigned freadle32_file(FILE *index);
static void linkedseek(unsigned pos);
static void linkedread(void *buffer, int length);
static unsigned linkedreadle32(void);

void io_setfilemode(int usedf)
{
	io_usedatafile = usedf;
}

int io_openlinkeddatafile(unsigned char *ptr)
{
	int index;

	if (datafilehandle)
		fclose(datafilehandle);
	datafilehandle = nullptr;

	datafilestart = ptr;
	linkedseek(0);

	linkedread(ident, 4);
	if (memcmp(ident, idstring, 4))
	{
		bme_error = BME_WRONG_FORMAT;
		return BME_ERROR;
	}

	files = linkedreadle32();
	fileheaders = (HEADER *)malloc(files * sizeof(HEADER));
	if (!fileheaders)
	{
		bme_error = BME_OUT_OF_MEMORY;
		return BME_ERROR;
	}
	for (index = 0; index < (int)files; index++)
	{
		fileheaders[index].offset = linkedreadle32();
		fileheaders[index].length = (Sint32)linkedreadle32();
		linkedread(&fileheaders[index].name, 13);
	}

	for (index = 0; index < MAX_HANDLES; index++)
		handle[index].open = 0;
	io_usedatafile = 1;
	bme_error = BME_OK;
	return BME_OK;
}

int io_opendatafile(char *name)
{
	int index;

	if (name)
	{
		datafilehandle = fopen(name, "rb");
		if (!datafilehandle)
		{
			bme_error = BME_OPEN_ERROR;
			return BME_ERROR;
		}
	}

	fread(ident, 4, 1, datafilehandle);
	if (memcmp(ident, idstring, 4))
	{
		bme_error = BME_WRONG_FORMAT;
		return BME_ERROR;
	}

	files = freadle32_file(datafilehandle);
	fileheaders = (HEADER *)malloc(files * sizeof(HEADER));
	if (!fileheaders)
	{
		bme_error = BME_OUT_OF_MEMORY;
		return BME_ERROR;
	}
	for (index = 0; index < (int)files; index++)
	{
		fileheaders[index].offset = freadle32_file(datafilehandle);
		fileheaders[index].length = (Sint32)freadle32_file(datafilehandle);
		fread(&fileheaders[index].name, 13, 1, datafilehandle);
	}

	for (index = 0; index < MAX_HANDLES; index++)
		handle[index].open = 0;
	io_usedatafile = 1;
	bme_error = BME_OK;
	return BME_OK;
}

int io_open(char *name)
{
	if (!name)
		return -1;

	if (!io_usedatafile)
	{
		int index;
		for (index = 0; index < MAX_HANDLES; index++)
		{
			if (!fileptr[index])
				break;
		}
		if (index == MAX_HANDLES)
			return -1;

		FILE *file = fopen(name, "rb");
		if (!file)
			return -1;
		fileptr[index] = file;
		return index;
	}

	int index;
	int namelength;
	char namecopy[13];

	namelength = (int)strlen(name);
	if (namelength > 12)
		namelength = 12;
	memcpy(namecopy, name, (size_t)namelength + 1);
	for (index = 0; index < (int)strlen(namecopy); index++)
		namecopy[index] = (char)toupper((unsigned char)namecopy[index]);

	for (index = 0; index < MAX_HANDLES; index++)
	{
		if (!handle[index].open)
		{
			int count = (int)files;
			handle[index].currentheader = fileheaders;

			while (count)
			{
				if (!strcmp(namecopy, handle[index].currentheader->name))
				{
					handle[index].open = 1;
					handle[index].filepos = 0;
					return index;
				}
				count--;
				handle[index].currentheader++;
			}
			return -1;
		}
	}
	return -1;
}

int io_lseek(int index, int offset, int whence)
{
	if (!io_usedatafile)
	{
		fseek(fileptr[index], offset, whence);
		return (int)ftell(fileptr[index]);
	}

	int newpos;

	if ((index < 0) || (index >= MAX_HANDLES))
		return -1;

	if (!handle[index].open)
		return -1;
	switch (whence)
	{
	default:
	case SEEK_SET:
		newpos = offset;
		break;
	case SEEK_CUR:
		newpos = offset + handle[index].filepos;
		break;
	case SEEK_END:
		newpos = offset + handle[index].currentheader->length;
		break;
	}
	if (newpos < 0)
		newpos = 0;
	if (newpos > handle[index].currentheader->length)
		newpos = handle[index].currentheader->length;
	handle[index].filepos = newpos;
	return newpos;
}

int io_read(int index, void *buffer, int length)
{
	if (!io_usedatafile)
		return (int)fread(buffer, 1, (size_t)length, fileptr[index]);

	int readbytes;

	if ((index < 0) || (index >= MAX_HANDLES))
		return -1;

	if (!handle[index].open)
		return -1;
	if (length + handle[index].filepos > handle[index].currentheader->length)
		length = handle[index].currentheader->length - handle[index].filepos;

	if (datafilehandle)
	{
		fseek(datafilehandle, handle[index].currentheader->offset + handle[index].filepos, SEEK_SET);
		readbytes = (int)fread(buffer, 1, (size_t)length, datafilehandle);
	}
	else
	{
		linkedseek(handle[index].currentheader->offset + (unsigned)handle[index].filepos);
		linkedread(buffer, length);
		readbytes = length;
	}
	handle[index].filepos += readbytes;
	return readbytes;
}

void io_close(int index)
{
	if (!io_usedatafile)
	{
		fclose(fileptr[index]);
		fileptr[index] = nullptr;
		return;
	}

	if ((index < 0) || (index >= MAX_HANDLES))
		return;
	handle[index].open = 0;
}

unsigned io_read8(int index)
{
	unsigned char byte;
	io_read(index, &byte, 1);
	return byte;
}

unsigned io_readle16(int index)
{
	unsigned char bytes[2];
	io_read(index, bytes, 2);
	return ((unsigned)bytes[1] << 8) | bytes[0];
}

unsigned io_readhe16(int index)
{
	unsigned char bytes[2];
	io_read(index, bytes, 2);
	return ((unsigned)bytes[0] << 8) | bytes[1];
}

unsigned io_readle32(int index)
{
	unsigned char bytes[4];
	io_read(index, bytes, 4);
	return ((unsigned)bytes[3] << 24) | ((unsigned)bytes[2] << 16) |
	       ((unsigned)bytes[1] << 8) | bytes[0];
}

unsigned io_readhe32(int index)
{
	unsigned char bytes[4];
	io_read(index, bytes, 4);
	return ((unsigned)bytes[0] << 24) | ((unsigned)bytes[1] << 16) |
	       ((unsigned)bytes[2] << 8) | bytes[3];
}

static unsigned freadle32_file(FILE *file)
{
	unsigned char bytes[4];
	fread(&bytes, 4, 1, file);
	return ((unsigned)bytes[3] << 24) | ((unsigned)bytes[2] << 16) |
	       ((unsigned)bytes[1] << 8) | bytes[0];
}

static void linkedseek(unsigned pos)
{
	datafileptr = &datafilestart[pos];
}

static void linkedread(void *buffer, int length)
{
	unsigned char *dest = (unsigned char *)buffer;
	while (length--)
		*dest++ = *datafileptr++;
}

static unsigned linkedreadle32(void)
{
	unsigned char bytes[4];
	linkedread(&bytes, 4);
	return ((unsigned)bytes[3] << 24) | ((unsigned)bytes[2] << 16) |
	       ((unsigned)bytes[1] << 8) | bytes[0];
}
