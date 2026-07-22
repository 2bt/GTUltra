#pragma once

#include "gplay.hpp"

// Visible order-list page size (expanded view scroll).
constexpr int EXTENDEDVISIBLEORDERLIST = 13;

extern const char* notename[];
extern const char* notenameTableView[];
extern char        timechar[];

void displayupdate(GTOBJECT* gt);
void resettime(GTOBJECT* gt);
void incrementtime(GTOBJECT* gt);
void setSongLengthTime(GTOBJECT* gt);
void setSIDTracker64KeyOnStyle();
void do_display(GTOBJECT* gt);
