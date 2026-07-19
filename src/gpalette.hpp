#pragma once

#include <array>
#include <string>

// Palette preset boot loading (M6 Phase 2 — extracted from gpaletteeditor).
// .gtp presets still load into paletteRGB for setSkin boot colours.

#ifndef MAX_PALETTE_PRESETS
#define MAX_PALETTE_PRESETS 16
#endif

extern std::array<std::string, MAX_PALETTE_PRESETS> paletteNames;
extern int currentLoadedPresetIndex;

int loadPalettes();
int readPaletteData(char* paletteMem, char* paletteName);
int loadPalette(char* palettePath, char* paletteFileName);
void setPaletteName(char* paletteName, int index);
