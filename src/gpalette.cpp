//
// Palette preset boot loading (M6 Phase 2 — extracted from gpaletteeditor).
//

#define GPALETTE_C

#include "goattrk2.hpp"
#include "gpalette.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

std::array<std::string, MAX_PALETTE_PRESETS> paletteNames;
int currentLoadedPresetIndex = 0;

static char paletteFile[256];
static char paletteStringBuffer[MAX_PATHNAME];
static struct dirent* paletteFolderEntry;

static char* sgets(char* s, int n, char** strp)
{
	if (**strp == 0)
		return nullptr;
	int i;
	for (i = 0; i < n - 1; ++i, ++(*strp)) {
		s[i] = **strp;
		if (**strp == 0)
			break;
		if ((**strp == 0xa) || (**strp == 0xd)) {
			s[i] = '\n';
			s[i + 1] = '\0';
			while (**strp <= 0xd) {
				if (**strp == 0)
					break;
				++(*strp);
			}
			break;
		}
	}
	if (i == n - 1)
		s[i] = '\0';
	return s;
}

static int convertStringToHex(char* str)
{
	int value = 0;
	for (;;) {
		char c = (char)tolower(*str++);
		int h = -1;
		if ((c >= 'a') && (c <= 'f'))
			h = c - 'a' + 10;
		if ((c >= '0') && (c <= '9'))
			h = c - '0';
		if (h >= 0) {
			value *= 16;
			value += h;
		} else
			break;
	}
	return value;
}

void setPaletteName(char* paletteName, int index)
{
	paletteNames[index] = paletteName;
}

int readPaletteData(char* paletteMem, char* paletteName)
{
	int foundPaletteInfo = 0;
	setPaletteName(paletteName, currentLoadedPresetIndex);
	char** p = &paletteMem;

	for (;;) {
		if (sgets(paletteStringBuffer, MAX_PATHNAME, p) == nullptr) {
			if (foundPaletteInfo)
				currentLoadedPresetIndex++;
			return 1;
		}

		if (foundPaletteInfo == 0) {
			if (strcmp(paletteStringBuffer, "PALETTEDATA:\n") == 0)
				foundPaletteInfo++;
		} else {
			char* token = strtok(paletteStringBuffer, ":");
			if (token != nullptr) {
				int paletteIndex = convertStringToHex(token);
				token = strtok(nullptr, ",");
				if (token == nullptr)
					break;
				int red = convertStringToHex(token);
				token = strtok(nullptr, ",");
				if (token == nullptr)
					break;
				int green = convertStringToHex(token);
				token = strtok(nullptr, "\t");
				if (token == nullptr)
					break;
				int blue = convertStringToHex(token);

				paletteRGB[currentLoadedPresetIndex][0][paletteIndex] = (unsigned char)red;
				paletteRGB[currentLoadedPresetIndex][1][paletteIndex] = (unsigned char)green;
				paletteRGB[currentLoadedPresetIndex][2][paletteIndex] = (unsigned char)blue;
			}
		}
	}

	if (foundPaletteInfo)
		currentLoadedPresetIndex++;
	return 0;
}

int loadPalette(char* palettePath, char* paletteFileName)
{
	if (currentLoadedPresetIndex >= MAX_PALETTE_PRESETS)
		return -1;

	FILE* handle = fopen(palettePath, "rb");
	if (handle == nullptr)
		return 0;

	fseek(handle, 0, SEEK_END);
	int size = (int)ftell(handle);
	fseek(handle, 0, SEEK_SET);

	char* paletteMem = (char*)malloc((size_t)size + 1);
	fread(paletteMem, (size_t)size, 1, handle);
	fclose(handle);
	paletteMem[size] = 0;

	int ret = readPaletteData(paletteMem, paletteFileName);
	free(paletteMem);
	return ret;
}

int loadPalettes()
{
	DIR* folder;

#ifdef __WIN32__
	createFilename(appFileName, paletteFile, "gtpalettes");
#else
	strcpy(paletteFile, getenv("HOME"));
	strcat(paletteFile, "/.goattrk/gtpalettes");
#endif

	folder = opendir(paletteFile);
	if (folder == nullptr) {
#ifdef __WIN32__
		mkdir(paletteFile);
#else
		mkdir(paletteFile, 0777);
#endif
		return 0;
	}

	while ((paletteFolderEntry = readdir(folder))) {
		if (paletteFolderEntry->d_name[0] == '.')
			continue;

#ifdef __WIN32__
		createFilename(appFileName, paletteFile, "gtpalettes");
		strcat(paletteFile, "\\");
#else
		strcpy(paletteFile, getenv("HOME"));
		strcat(paletteFile, "/.goattrk/gtpalettes/");
#endif
		strcat(paletteFile, paletteFolderEntry->d_name);
		loadPalette(paletteFile, paletteFolderEntry->d_name);
	}

	closedir(folder);
	return 0;
}
