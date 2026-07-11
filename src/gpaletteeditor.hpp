#ifndef GPALETTEEDITOR_H
#define GPALETTEEDITOR_H

#include <array>
#include <string>

// Names of the 16 palette presets. Empty string == undefined slot.
// (16 == MAX_PALETTE_PRESETS, spelled literally here because that macro is
// defined in goattrk2.h *after* this header is included.)
extern std::array<std::string, 16> paletteNames;

#ifndef GPALETTEEDITOR_C

extern char* paletteText[];
extern struct dirent *paletteFolderEntry;
extern char paletteFile[256];
extern char paletteStringBuffer[MAX_PATHNAME];
extern int currentLoadedPresetIndex;
#endif

void displayPaletteEditorWindow(GTOBJECT *gt);
int getPaletteTextArraySize();
int paletteEdit(int *cx, int *cy, GTOBJECT *gt);
void changePalettePreset(int change, GTOBJECT *gt);
void process32EntryPalette(int maxPresets, int maxPaletteEntries, char* tempPalette);
void copyRGBInfo();
void handlePaste(int *cx, GTOBJECT *gt);
void rememberCurrentRGB(int *cx);
int savePalette(GTOBJECT *gt);
void convert4BitPaletteTo8Bit();
int convertStringToHex(char *str);
int loadPalette(char *palettePath, char *paletteFileName);
int loadPalettes();
int savePaletteText();
int readPaletteData(char *paletteMem, char *paletteName);
char *sgets(char *s, int n, char **strp);
void setPaletteName(char* paletteName, int index);

#endif
