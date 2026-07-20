#pragma once

#include "gplay.hpp"

enum GTFKEY_ANDOR { FKEYS_AND = 0, FKEYS_OR, FKEYS_NONE };

struct GTFKEY_ENTRY {
    int   key;
    int   shift;
    int   ctrl;
    int   andOr;
    char* actionList;
};

int fkeys_check(GTOBJECT* gt, int rawkey);
int fkeys_loadCFG();
