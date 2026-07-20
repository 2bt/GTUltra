#include "glegacy_ui.hpp"

void clearscreen(int) {}
void printtext(int, int, int, const char*) {}
void printblankc(int, int, int, int) {}
int  getColor(int fcolor, int bcolor) { return fcolor | (bcolor << 8); }
