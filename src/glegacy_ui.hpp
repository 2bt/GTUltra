#pragma once

// No-op leftovers for relocator interactive screens / stray debug prints.
// Remove when those call sites are deleted.

#define MAX_COLUMNS 100
#define MAX_ROWS 41

void clearscreen(int back_color);
void printtext(int x, int y, int color, const char* text);
void printblankc(int x, int y, int color, int length);
int  getColor(int fcolor, int bcolor);
