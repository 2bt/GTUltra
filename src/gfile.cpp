//
// GTUltra path helpers (M6 Phase 2 — chargen fileselector removed).
// File dialogs go through gfiledialog / portable-file-dialogs.
//

#define GFILE_C

#include "goattrk2.hpp"

#include <cstdio>

#ifdef __WIN32__
#include <windows.h>
#endif

DIRENTRY direntry[MAX_DIRFILES];

void initpaths(void)
{
	int c;

	for (c = 0; c < MAX_DIRFILES; c++)
		direntry[c].name = NULL;

	memset(loadedsongfilename, 0, sizeof loadedsongfilename);
	memset(songfilename, 0, sizeof songfilename);
	memset(instrfilename, 0, sizeof instrfilename);
	memset(songpath, 0, sizeof songpath);
	memset(instrpath, 0, sizeof instrpath);
	memset(packedpath, 0, sizeof packedpath);
	strcpy(songfilter, "*.sng");
	strcpy(wavfilter, "*.wav");
	strcpy(instrfilter, "*.ins");
	strcpy(palettefilter, "*.gtp");

	getcwd(songpath, MAX_PATHNAME);
	strcpy(instrpath, songpath);
	strcpy(packedpath, songpath);
}

int fileselector(char* name, char* path, char* filter, char* title, int filemode, GTOBJECT* gt, int boxColor, int miscFlags)
{
	(void)name;
	(void)path;
	(void)filter;
	(void)filemode;
	(void)gt;
	(void)boxColor;
	(void)miscFlags;
	// M6: chargen file selector removed. Callers should use gtfile:: dialogs.
	std::fprintf(stderr, "legacy fileselector stub called (%s)\n", title ? title : "?");
	return 0;
}

void editstring(char* buffer, int maxlength)
{
	(void)buffer;
	(void)maxlength;
	// Legacy text-cell string editor — unused under ImGui-only UI.
}

int cmpname(char* string1, char* string2)
{
	(void)string1;
	(void)string2;
	return 0;
}
